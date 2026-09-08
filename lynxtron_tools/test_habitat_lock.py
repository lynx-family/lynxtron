# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

import os
import tempfile
import unittest
from unittest import mock

from lynxtron_tools import habitat_lock


class HabitatCacheLockTest(unittest.TestCase):

    def test_excludes_another_writer_and_releases_the_lock(self):
        with tempfile.TemporaryDirectory() as cache_dir, mock.patch.dict(
            os.environ, {"HABITAT_CACHE_DIR": cache_dir}
        ):
            lock_path = os.path.join(cache_dir, ".sync.lock")
            with habitat_lock.habitat_cache_lock("first writer"):
                with open(lock_path, "r+b") as second_writer:
                    self.assertFalse(habitat_lock._try_lock(second_writer))

            with open(lock_path, "r+b") as second_writer:
                self.assertTrue(habitat_lock._try_lock(second_writer))
                habitat_lock._unlock(second_writer)


if __name__ == "__main__":
    unittest.main()
