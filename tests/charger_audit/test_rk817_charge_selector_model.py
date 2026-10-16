"""Host-only selector-level audit, NOT PMIC hardware testing.

This deliberately models the current public flip_rk817_charger.c algorithm.
Green results demonstrate a known cap violation can be reproduced in the
decision logic, NOT that the production charging implementation is safe.
"""
import unittest

CURRENTS = (1000, 1500, 2000, 2500, 2750, 3000, 3500, 500)
SELECTOR = {ma: i for i, ma in enumerate(CURRENTS)}


class PublishedHelperModel:
    def __init__(self, initial_ma):
        self.selector = SELECTOR[initial_ma]
        self.restore_selector = self.selector
        self.override_active = False

    @property
    def ma(self):
        return CURRENTS[self.selector]

    def vendor_select(self, ma):
        self.selector = SELECTOR[ma]

    def request(self, ma):
        current_selector = self.selector
        if ma == -1:
            if self.override_active:
                self.selector = self.restore_selector
                self.override_active = False
            return
        baseline_selector = self.restore_selector if self.override_active else current_selector
        target_selector = SELECTOR[min(CURRENTS[baseline_selector], ma)]
        if not self.override_active and target_selector == current_selector:
            return
        if not self.override_active:
            self.restore_selector = current_selector
            self.override_active = True
        self.selector = target_selector


class RK817ChargingAuditModel(unittest.TestCase):
    def test_ordinary_thermal_caps(self):
        helper = PublishedHelperModel(2500)
        helper.request(1500)
        self.assertEqual(helper.ma, 1500)
        helper.request(1000)
        self.assertEqual(helper.ma, 1000)

    def test_first_cap_preserves_existing_vendor_minimum(self):
        helper = PublishedHelperModel(500)
        helper.request(1500)
        self.assertEqual(helper.ma, 500)
        self.assertFalse(helper.override_active)

    def test_known_counterexample_vendor_lowering_is_undone(self):
        helper = PublishedHelperModel(2500)
        helper.request(1500)
        helper.vendor_select(500)
        helper.request(1500)
        # Existing algorithm conditionally raises the actual selector to 1.5 A.
        self.assertEqual(helper.ma, 1500)
        self.assertGreater(helper.ma, 500)

    def test_known_counterexample_release_restores_stale_baseline(self):
        helper = PublishedHelperModel(2500)
        helper.request(1500)
        helper.vendor_select(500)
        helper.request(-1)
        self.assertEqual(helper.ma, 2500)
        self.assertGreater(helper.ma, 500)

    def test_reassert_cap_when_vendor_sets_higher_current(self):
        helper = PublishedHelperModel(2500)
        helper.request(1000)
        helper.vendor_select(3500)
        helper.request(1000)
        self.assertEqual(helper.ma, 1000)

    def test_pause_label_still_sets_nonzero_current(self):
        helper = PublishedHelperModel(2500)
        helper.request(500)
        self.assertEqual(helper.selector, 7)
        self.assertEqual(helper.ma, 500)


if __name__ == "__main__":
    unittest.main()
