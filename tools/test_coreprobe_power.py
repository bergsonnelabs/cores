#!/usr/bin/env python3
"""Tests for the target-power decision rules.

`decide()` is pure so the safety rules can be checked without a probe or a
target attached — which matters, because the failure they prevent is
destructive and cannot be exercised on the bench.
"""
import unittest

from coreprobe_power import PowerError, classify, decide, read_declaration


class TestClassify(unittest.TestCase):
    """Bands from CoreProbe docs/level-shifter.md."""

    def test_bands(self):
        self.assertEqual(classify(8), "absent")
        self.assertEqual(classify(1791), "1v8")
        self.assertEqual(classify(3295), "3v3")

    def test_between_the_classes_is_anomalous_not_rounded(self):
        # The sense reads SWDIO held up by the target's own pull-up, so a real
        # target sits near a rail. 1027 mV is what an unpowered pull-up reads;
        # calling it "1v8" would drive signalling at a board that is not up.
        self.assertEqual(classify(1027), "anomalous")
        self.assertEqual(classify(2500), "anomalous")
        self.assertEqual(classify(3601), "anomalous")


class TestDecide(unittest.TestCase):
    def test_self_powered_target_is_never_fed(self):
        # Even when the project declares a supply. The three load switches share
        # T.V+ and the hardware has no contention protection, so a board with a
        # rail of its own must never be driven.
        self.assertEqual(decide(3295, "3v3", "3v3"), (None, "3v3"))
        self.assertEqual(decide(1791, "5v", "3v3"), (None, "1v8"))

    def test_self_powered_level_comes_from_the_sense(self):
        self.assertEqual(decide(3295, None, None), (None, "3v3"))

    def test_unpowered_and_undeclared_refuses_with_the_json_to_add(self):
        with self.assertRaises(PowerError) as cm:
            decide(8, None, None)
        msg = str(cm.exception)
        self.assertIn("target_power", msg)
        self.assertIn("target_logic", msg)

    def test_unpowered_and_declared_supplies_exactly_that(self):
        self.assertEqual(decide(8, "3v3", "1v8"), ("3v3", "1v8"))

    def test_supply_without_a_logic_level_is_refused(self):
        # The two are independent: a regulated board fed 5 V runs 3V3 logic, so
        # the supply cannot imply the level.
        with self.assertRaises(PowerError) as cm:
            decide(8, "3v3", None)
        self.assertIn("target_logic", str(cm.exception))

    def test_five_volts_is_refused_with_the_reason(self):
        with self.assertRaises(PowerError) as cm:
            decide(8, "5v", "3v3")
        self.assertIn("back-feeds", str(cm.exception))

    def test_nonsense_supply_is_refused(self):
        with self.assertRaises(PowerError):
            decide(8, "12v", "3v3")

    def test_anomalous_self_powered_target_is_reported_not_guessed(self):
        with self.assertRaises(PowerError) as cm:
            decide(2500, None, None)
        self.assertIn("neither 1V8", str(cm.exception))


class TestReadDeclaration(unittest.TestCase):
    def test_missing_file_reads_as_undeclared(self):
        # A project with no config.json is self-powered by default, not an error.
        self.assertEqual(read_declaration("/nonexistent/config.json"), (None, None))

    def test_absent_block_reads_as_undeclared(self):
        import json
        import tempfile

        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
            json.dump({"core": "Core.ST.W5"}, f)
            path = f.name
        self.assertEqual(read_declaration(path), (None, None))

    def test_block_is_read_back(self):
        import json
        import tempfile

        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
            json.dump({"probe": {"target_power": "3v3", "target_logic": "1v8"}}, f)
            path = f.name
        self.assertEqual(read_declaration(path), ("3v3", "1v8"))


if __name__ == "__main__":
    unittest.main()
