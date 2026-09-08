# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

"""Protect one Habitat bare Git cache while a remote fetch is in progress."""

import glob
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import time


SNAPSHOT_SUFFIX = ".lynxtron-fetch-snapshot"
SNAPSHOT_MAX_AGE_SECONDS = 24 * 60 * 60


def _git_cache_root(cache_dir):
    return os.path.realpath(os.path.join(cache_dir, "git"))


def _is_habitat_bare_cache(repo_dir, cache_dir):
    repo_dir = os.path.realpath(repo_dir)
    git_root = _git_cache_root(cache_dir)
    try:
        if os.path.commonpath((repo_dir, git_root)) != git_root:
            return False
    except ValueError:
        return False
    return (
        os.path.isfile(os.path.join(repo_dir, "HEAD"))
        and os.path.isdir(os.path.join(repo_dir, "objects"))
    )


def _hardlink_tree(source, destination):
    """Create a metadata-only tree snapshot; never fall back to copying data."""
    os.makedirs(destination)
    try:
        for root, directories, files in os.walk(source):
            relative_root = os.path.relpath(root, source)
            target_root = (
                destination
                if relative_root == os.curdir
                else os.path.join(destination, relative_root)
            )
            for directory in directories:
                source_path = os.path.join(root, directory)
                target_path = os.path.join(target_root, directory)
                if os.path.islink(source_path):
                    os.symlink(os.readlink(source_path), target_path)
                else:
                    os.mkdir(target_path)
            for filename in files:
                source_path = os.path.join(root, filename)
                target_path = os.path.join(target_root, filename)
                if os.path.islink(source_path):
                    os.symlink(os.readlink(source_path), target_path)
                elif stat.S_ISREG(os.stat(source_path, follow_symlinks=False).st_mode):
                    os.link(source_path, target_path)
                else:
                    raise OSError(f"unsupported file in Git cache: {source_path}")
    except Exception:
        shutil.rmtree(destination, ignore_errors=True)
        raise


def create_snapshot(repo_dir, cache_dir):
    """Snapshot a single existing Habitat Git repo using same-filesystem links."""
    if not _is_habitat_bare_cache(repo_dir, cache_dir):
        return None

    snapshot = repo_dir + SNAPSHOT_SUFFIX
    if os.path.exists(snapshot):
        return snapshot

    parent = os.path.dirname(repo_dir)
    temporary = tempfile.mkdtemp(prefix=".lynxtron-fetch-snapshot-", dir=parent)
    os.rmdir(temporary)
    try:
        _hardlink_tree(repo_dir, temporary)
        os.replace(temporary, snapshot)
        print(f"Protected Habitat Git cache before remote fetch: {repo_dir}")
        return snapshot
    except OSError as error:
        shutil.rmtree(temporary, ignore_errors=True)
        print(
            "Warning: unable to create hardlink snapshot for Habitat Git cache "
            f"{repo_dir}: {error}. Continuing without a full-copy fallback.",
            file=sys.stderr,
        )
        return None


def discard_snapshot(snapshot):
    if snapshot and os.path.isdir(snapshot):
        shutil.rmtree(snapshot)


def _restore_snapshot(snapshot):
    repo_dir = snapshot[: -len(SNAPSHOT_SUFFIX)]
    discarded = repo_dir + ".lynxtron-fetch-discarded"
    shutil.rmtree(discarded, ignore_errors=True)
    if os.path.lexists(repo_dir):
        os.replace(repo_dir, discarded)
    try:
        os.replace(snapshot, repo_dir)
    except Exception:
        if not os.path.lexists(repo_dir) and os.path.lexists(discarded):
            os.replace(discarded, repo_dir)
        raise
    shutil.rmtree(discarded, ignore_errors=True)
    print(f"Restored Habitat Git cache after failed remote fetch: {repo_dir}")


def restore_pending_snapshots(cache_dir):
    """Recover failed/crashed fetch transactions and prune malformed old state."""
    git_root = _git_cache_root(cache_dir)
    patterns = (
        os.path.join(glob.escape(git_root), "*", "*" + SNAPSHOT_SUFFIX),
        os.path.join(glob.escape(git_root), "*", ".lynxtron-fetch-snapshot-*"),
    )
    for pattern in patterns:
        for snapshot in glob.glob(pattern):
            if snapshot.endswith(SNAPSHOT_SUFFIX):
                try:
                    _restore_snapshot(snapshot)
                except OSError as error:
                    print(
                        "Warning: unable to restore Habitat Git cache snapshot "
                        f"{snapshot}: {error}",
                        file=sys.stderr,
                    )
            else:
                try:
                    age = time.time() - os.path.getmtime(snapshot)
                    if age >= SNAPSHOT_MAX_AGE_SECONDS:
                        shutil.rmtree(snapshot, ignore_errors=True)
                except OSError:
                    pass


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    real_git = os.environ.get("LYNXTRON_REAL_GIT")
    cache_dir = os.environ.get("LYNXTRON_HABITAT_CACHE_DIR")
    if not real_git:
        print("LYNXTRON_REAL_GIT is not configured", file=sys.stderr)
        return 127

    snapshot = None
    if argv and argv[0] == "fetch" and cache_dir:
        snapshot = create_snapshot(os.getcwd(), cache_dir)

    return_code = subprocess.call([real_git] + argv)
    if return_code == 0:
        discard_snapshot(snapshot)
    # On failure, leave the snapshot outside the repository. Habitat may now
    # delete its cache directory; the outer locked transaction restores it.
    return return_code


if __name__ == "__main__":
    sys.exit(main())
