# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

"""Serialize writers to the Habitat cache shared by CI runners."""

import contextlib
import os
import shutil
import subprocess
import sys
import time

try:
    from lynxtron_tools.git_cache_guard import restore_pending_snapshots
except ModuleNotFoundError:
    # habitat_lock.py is also invoked directly while the current directory is
    # src/, in which case only lynxtron_tools/ itself is on sys.path.
    from git_cache_guard import restore_pending_snapshots


DEFAULT_LOCK_TIMEOUT_SECONDS = 30 * 60
LOCK_POLL_INTERVAL_SECONDS = 2


def _cache_dir():
    configured_dir = os.environ.get("HABITAT_CACHE_DIR", "~/.habitat_cache")
    return os.path.realpath(os.path.expandvars(os.path.expanduser(configured_dir)))


def _try_lock(lock_file):
    lock_file.seek(0)
    if os.name == "nt":
        import msvcrt

        try:
            msvcrt.locking(lock_file.fileno(), msvcrt.LK_NBLCK, 1)
            return True
        except OSError:
            return False

    import fcntl

    try:
        fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        return True
    except BlockingIOError:
        return False


def _unlock(lock_file):
    lock_file.seek(0)
    if os.name == "nt":
        import msvcrt

        msvcrt.locking(lock_file.fileno(), msvcrt.LK_UNLCK, 1)
        return

    import fcntl

    fcntl.flock(lock_file.fileno(), fcntl.LOCK_UN)


@contextlib.contextmanager
def _git_cache_guard_environment(cache_dir):
    """Route Habitat's Git commands through the per-repository cache guard."""
    real_git = shutil.which("git")
    if not real_git:
        raise RuntimeError("git executable not found while preparing Habitat cache guard")

    wrapper_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), "git_cache_guard_bin")
    previous = {
        name: os.environ.get(name)
        for name in ("PATH", "LYNXTRON_REAL_GIT", "LYNXTRON_HABITAT_CACHE_DIR")
    }
    os.environ["LYNXTRON_REAL_GIT"] = real_git
    os.environ["LYNXTRON_HABITAT_CACHE_DIR"] = cache_dir
    os.environ["PATH"] = wrapper_dir + os.pathsep + (previous["PATH"] or "")

    restore_pending_snapshots(cache_dir)
    try:
        yield
    finally:
        # A failed Git fetch leaves its snapshot in place because Habitat deletes
        # the repository after Git exits. Restore only after Habitat has returned.
        restore_pending_snapshots(cache_dir)
        for name, value in previous.items():
            if value is None:
                os.environ.pop(name, None)
            else:
                os.environ[name] = value


@contextlib.contextmanager
def habitat_cache_lock(description="Habitat sync"):
    """Hold an inter-process lock while a command may mutate Habitat's cache."""
    cache_dir = _cache_dir()
    os.makedirs(cache_dir, exist_ok=True)
    lock_path = os.path.join(cache_dir, ".sync.lock")
    timeout = int(
        os.environ.get(
            "HABITAT_CACHE_LOCK_TIMEOUT_SECONDS", DEFAULT_LOCK_TIMEOUT_SECONDS
        )
    )
    start = time.monotonic()

    with open(lock_path, "a+b") as lock_file:
        if os.path.getsize(lock_path) == 0:
            lock_file.write(b"\0")
            lock_file.flush()

        announced_wait = False
        while not _try_lock(lock_file):
            if not announced_wait:
                print(f"Waiting for shared Habitat cache lock: {lock_path}")
                announced_wait = True
            if time.monotonic() - start >= timeout:
                raise TimeoutError(
                    f"Timed out after {timeout}s waiting for Habitat cache lock: "
                    f"{lock_path}"
                )
            time.sleep(LOCK_POLL_INTERVAL_SECONDS)

        print(f"Acquired shared Habitat cache lock for {description}: {lock_path}")
        try:
            with _git_cache_guard_environment(cache_dir):
                yield
        finally:
            _unlock(lock_file)
            print(f"Released shared Habitat cache lock for {description}: {lock_path}")


def main(argv):
    command = argv[1:]
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        print("usage: habitat_lock.py -- command [args ...]", file=sys.stderr)
        return 2

    with habitat_cache_lock(" ".join(command)):
        return subprocess.call(command)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
