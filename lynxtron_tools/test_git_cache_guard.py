# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import os
import shutil
import tempfile
import unittest
from unittest import mock

from lynxtron_tools import git_cache_guard


class GitCacheGuardTest(unittest.TestCase):

    def _make_cache_repo(self, cache_dir):
        repo_dir = os.path.join(cache_dir, "git", "example.git", "url-hash")
        objects_dir = os.path.join(repo_dir, "objects", "aa")
        os.makedirs(objects_dir)
        with open(os.path.join(repo_dir, "HEAD"), "w", encoding="utf-8") as output:
            output.write("ref: refs/heads/main\n")
        object_path = os.path.join(objects_dir, "object")
        with open(object_path, "wb") as output:
            output.write(b"cached object")
        return repo_dir, object_path

    def test_successful_fetch_discards_repo_snapshot(self):
        with tempfile.TemporaryDirectory() as cache_dir:
            repo_dir, object_path = self._make_cache_repo(cache_dir)
            with mock.patch.dict(
                os.environ,
                {
                    "LYNXTRON_REAL_GIT": "/real/git",
                    "LYNXTRON_HABITAT_CACHE_DIR": cache_dir,
                },
            ), mock.patch.object(
                git_cache_guard.subprocess, "call", return_value=0
            ), mock.patch.object(git_cache_guard.os, "getcwd", return_value=repo_dir):
                result = git_cache_guard.main(["fetch", "origin", "revision"])

            self.assertEqual(result, 0)
            self.assertTrue(os.path.isfile(object_path))
            self.assertFalse(os.path.exists(repo_dir + git_cache_guard.SNAPSHOT_SUFFIX))

    def test_failed_fetch_is_restored_after_habitat_deletes_repo(self):
        with tempfile.TemporaryDirectory() as cache_dir:
            repo_dir, object_path = self._make_cache_repo(cache_dir)
            with mock.patch.dict(
                os.environ,
                {
                    "LYNXTRON_REAL_GIT": "/real/git",
                    "LYNXTRON_HABITAT_CACHE_DIR": cache_dir,
                },
            ), mock.patch.object(
                git_cache_guard.subprocess, "call", return_value=1
            ), mock.patch.object(git_cache_guard.os, "getcwd", return_value=repo_dir):
                result = git_cache_guard.main(["fetch", "origin", "revision"])

            snapshot = repo_dir + git_cache_guard.SNAPSHOT_SUFFIX
            self.assertEqual(result, 1)
            self.assertTrue(os.path.isdir(snapshot))
            shutil.rmtree(repo_dir)

            git_cache_guard.restore_pending_snapshots(cache_dir)

            self.assertFalse(os.path.exists(snapshot))
            with open(object_path, "rb") as cached_object:
                self.assertEqual(cached_object.read(), b"cached object")

    def test_non_fetch_git_command_does_not_snapshot(self):
        with tempfile.TemporaryDirectory() as cache_dir:
            repo_dir, _ = self._make_cache_repo(cache_dir)
            with mock.patch.dict(
                os.environ,
                {
                    "LYNXTRON_REAL_GIT": "/real/git",
                    "LYNXTRON_HABITAT_CACHE_DIR": cache_dir,
                },
            ), mock.patch.object(
                git_cache_guard.subprocess, "call", return_value=0
            ), mock.patch.object(git_cache_guard.os, "getcwd", return_value=repo_dir):
                git_cache_guard.main(["cat-file", "-t", "revision"])

            self.assertFalse(os.path.exists(repo_dir + git_cache_guard.SNAPSHOT_SUFFIX))

    def test_hardlink_failure_never_falls_back_to_copying_repo(self):
        with tempfile.TemporaryDirectory() as cache_dir:
            repo_dir, _ = self._make_cache_repo(cache_dir)
            with mock.patch.object(git_cache_guard.os, "link", side_effect=OSError("unsupported")):
                snapshot = git_cache_guard.create_snapshot(repo_dir, cache_dir)

            self.assertIsNone(snapshot)
            self.assertFalse(os.path.exists(repo_dir + git_cache_guard.SNAPSHOT_SUFFIX))


if __name__ == "__main__":
    unittest.main()
