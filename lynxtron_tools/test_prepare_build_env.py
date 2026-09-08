# Copyright 2026 The Lynxtron Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
import contextlib
import os
import unittest
from unittest import mock

from lynxtron_tools import prepare_build_env


class RunHabitatSyncTest(unittest.TestCase):

    def setUp(self):
        lock_patcher = mock.patch.object(
            prepare_build_env,
            "habitat_cache_lock",
            return_value=contextlib.nullcontext(),
        )
        lock_patcher.start()
        self.addCleanup(lock_patcher.stop)

    def test_preserves_configured_habitat_concurrency(self):
        with mock.patch.dict(
            prepare_build_env.os.environ,
            {"HABITAT_CONCURRENCY": "99"},
        ):
            prepare_build_env.configure_habitat_environment()

            self.assertEqual(
                prepare_build_env.os.environ["HABITAT_CONCURRENCY"], "99"
            )
            self.assertEqual(
                prepare_build_env.os.environ["GIT_LFS_SKIP_SMUDGE"], "1"
            )

    def test_defaults_to_two_concurrent_habitat_requests(self):
        with mock.patch.dict(prepare_build_env.os.environ, {}, clear=True):
            prepare_build_env.configure_habitat_environment()

            self.assertEqual(
                prepare_build_env.os.environ["HABITAT_CONCURRENCY"], "2"
            )

    def test_uses_checkout_relative_habitat_cache_in_github_actions(self):
        with mock.patch.dict(
            prepare_build_env.os.environ, {"GITHUB_ACTIONS": "true"}, clear=True
        ):
            prepare_build_env.configure_habitat_environment()

            self.assertEqual(
                prepare_build_env.os.environ["HABITAT_CACHE_DIR"],
                os.path.join(prepare_build_env.root_dir, ".habitat_cache"),
            )

    @mock.patch.object(prepare_build_env.time, "sleep")
    @mock.patch.object(prepare_build_env.os, "system", side_effect=[1, 2, 0])
    def test_retries_until_success_with_exponential_backoff(
        self, system_mock, sleep_mock
    ):
        result = prepare_build_env.run_habitat_sync("hab sync", "sync test")

        self.assertEqual(result, 0)
        self.assertEqual(system_mock.call_count, 3)
        sleep_mock.assert_has_calls([mock.call(10), mock.call(20)])

    @mock.patch.object(prepare_build_env.time, "sleep")
    @mock.patch.object(prepare_build_env.os, "system", return_value=7)
    def test_returns_last_failure_after_five_attempts(
        self, system_mock, sleep_mock
    ):
        result = prepare_build_env.run_habitat_sync("hab sync", "sync test")

        self.assertEqual(result, 7)
        self.assertEqual(system_mock.call_count, 5)
        sleep_mock.assert_has_calls(
            [mock.call(10), mock.call(20), mock.call(40), mock.call(80)]
        )

    @mock.patch.object(prepare_build_env.time, "sleep")
    @mock.patch.object(prepare_build_env.os, "system", return_value=0)
    def test_does_not_retry_success(self, system_mock, sleep_mock):
        result = prepare_build_env.run_habitat_sync("hab sync", "sync test")

        self.assertEqual(result, 0)
        system_mock.assert_called_once_with("hab sync")
        sleep_mock.assert_not_called()


if __name__ == "__main__":
    unittest.main()
