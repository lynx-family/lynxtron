"""Opt-in local POSIX artifact cache. Never reads other Habitat entries."""
import contextlib
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import tarfile
import tempfile

from native_cache_budget import plan_admission


def file_hash(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def input_key(roots, identity, excluded=(), progress=None):
    """Conservative full-tree content key, including previously absent headers.

    Only Git metadata is omitted. Callers must keep outputs outside input roots
    and supply every external dependency/toolchain and relevant environment.
    No cross-checkout reuse: absolute root identities are intentional.
    """
    records = []
    visited = set()
    excluded = {Path(p).resolve() for p in excluded}

    def visit(path, label, ancestors):
        if progress:
            progress(path)
        resolved = path.resolve(strict=True)
        if any(resolved == p or p in resolved.parents for p in excluded):
            return
        if resolved in ancestors:
            # Dependency trees (notably pnpm) can link back to an ancestor. Its
            # directory contents are already being traversed; retain the edge.
            records.append((label, "back-reference", str(resolved)))
            return
        if path.is_symlink():
            records.append((label, "link", os.readlink(path)))
        if resolved in visited:
            records.append((label, "alias", str(resolved)))
            return
        visited.add(resolved)
        if path.is_dir():
            records.append((label, "directory"))
            for child in sorted(path.iterdir()):
                if child.name != ".git":
                    visit(child, label + "/" + child.name, ancestors | {resolved})
        elif path.is_file():
            records.append((label, path.stat().st_mode & 0o777, file_hash(path)))
        else:
            raise ValueError(f"Unsupported input: {label}")

    for root in sorted(roots, key=str):
        path = Path(root).absolute()
        visit(path, str(path), set())
    data = json.dumps({"schema": 1, "inputs": records, "identity": identity},
                      sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(data.encode()).hexdigest()


class LocalCache:
    def __init__(self, root, max_bytes, max_entries):
        self.root = Path(root).absolute()
        self.max_bytes = max_bytes
        self.max_entries = max_entries
        plan_admission([], 0, max_bytes, max_entries)
        if self.root.is_symlink():
            raise ValueError("Cache root must not be a symlink")
        self.root.mkdir(parents=True, exist_ok=True)
        marker = self.root / ".native-cache-v1"
        if not marker.exists():
            if any(self.root.iterdir()):
                raise ValueError("Refusing to adopt a nonempty cache directory")
            marker.write_text("local-native-cache-v1\n")
        if marker.is_symlink() or marker.read_text() != "local-native-cache-v1\n":
            raise ValueError("Invalid cache ownership marker")

    @contextlib.contextmanager
    def locked(self):
        lock = self.root / ".lock"
        fd = os.open(lock, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
        with os.fdopen(fd, "w") as handle:
            fcntl.flock(handle, fcntl.LOCK_EX)
            yield

    def entry(self, key):
        if not re.fullmatch(r"[0-9a-f]{64}", key):
            raise ValueError("Invalid cache key")
        result = self.root / key
        if result.is_symlink():
            raise ValueError("Cache entry must not be a symlink")
        return result

    def entries(self):
        entries = []
        for item in self.root.iterdir():
            if item.name in (".lock", ".native-cache-v1"):
                continue
            path = self.entry(item.name)
            if not path.is_dir():
                raise ValueError("Unexpected cache entry")
            size = 0
            for child in path.iterdir():
                if child.is_symlink() or not child.is_file():
                    raise ValueError("Unexpected cache payload")
                size += child.stat().st_size
            entries.append((item.name, size, item.stat().st_mtime_ns))
        return entries

    def prune(self, incoming=0, extra_entry=0):
        entries = self.entries()
        # Include ownership/lock metadata in the byte limit.
        available = self.max_bytes - (self.root / ".native-cache-v1").stat().st_size
        if available <= 0 or incoming > available:
            return False
        victims = plan_admission(entries, incoming, available,
                                 self.max_entries + (1 - extra_entry))
        if victims is None:
            return False
        for key in victims:
            shutil.rmtree(self.entry(key))
        return True

    def put(self, key, source):
        self.entry(key)
        # Bound archive preparation too: stop writing once the budget is used.
        with tempfile.TemporaryDirectory(prefix="native-cache-stage-") as tmp:
            staged = Path(tmp)
            archive = staged / "artifact.tar"
            with archive.open("wb") as output:
                class LimitedWriter:
                    def write(inner, data):
                        if output.tell() + len(data) > self.max_bytes:
                            raise OverflowError("Artifact exceeds cache budget")
                        return output.write(data)
                try:
                    with tarfile.open(fileobj=LimitedWriter(), mode="w|") as tar:
                        tar.add(source, arcname="payload", recursive=True)
                except OverflowError:
                    return False
            (staged / "metadata.json").write_text(json.dumps({
                "key": key, "sha256": file_hash(archive)}))
            size = sum(p.stat().st_size for p in staged.iterdir())
            with self.locked():
                if size + (self.root / ".native-cache-v1").stat().st_size > self.max_bytes:
                    return False
                existing = self.entry(key)
                if existing.exists():
                    shutil.rmtree(existing)
                if not self.prune(size, 1):
                    return False
                existing.mkdir()
                try:
                    for path in staged.iterdir():
                        shutil.copyfile(path, existing / path.name)
                except BaseException:
                    shutil.rmtree(existing)
                    raise
            return True

    def restore(self, key, destination):
        destination = Path(destination)
        if destination.exists() or destination.is_symlink():
            raise ValueError("Restore destination must not exist")
        with self.locked():
            if not self.prune():
                return False
            entry = self.entry(key)
            if not entry.exists():
                return False
            try:
                metadata = json.loads((entry / "metadata.json").read_text())
                archive = entry / "artifact.tar"
                if metadata["key"] != key or metadata["sha256"] != file_hash(archive):
                    return False
                destination.parent.mkdir(parents=True, exist_ok=True)
                with tempfile.TemporaryDirectory(dir=destination.parent,
                                                 prefix="native-restore-") as tmp:
                    with tarfile.open(archive) as tar:
                        tar.extractall(tmp, filter="data")
                    os.replace(Path(tmp) / "payload", destination)
                os.utime(entry, None)
                return True
            except (OSError, ValueError, KeyError, tarfile.TarError):
                return False
