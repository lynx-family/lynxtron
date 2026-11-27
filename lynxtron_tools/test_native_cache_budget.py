import unittest

from native_cache_budget import plan_admission


class CacheBudgetTest(unittest.TestCase):
    def test_byte_limit_uses_least_recently_used(self):
        self.assertEqual(plan_admission(
            [("recent", 40, 2), ("old", 40, 1)], 50, 100, 10), ["old"])

    def test_entry_count_is_also_bounded(self):
        self.assertEqual(plan_admission(
            [("old", 1, 1), ("new", 1, 2)], 1, 100, 2), ["old"])

    def test_oversized_artifact_is_not_cached(self):
        self.assertIsNone(plan_admission([("keep", 1, 1)], 101, 100, 2))

    def test_repeated_admissions_stay_bounded(self):
        entries = []
        for index in range(100):
            size = index % 20 + 1
            evictions = plan_admission(entries, size, 50, 4)
            entries = [entry for entry in entries if entry[0] not in evictions]
            entries.append((str(index), size, index))
            self.assertLessEqual(sum(entry[1] for entry in entries), 50)
            self.assertLessEqual(len(entries), 4)

    def test_invalid_budget_rejected(self):
        with self.assertRaises(ValueError):
            plan_admission([], 1, 0, 1)
