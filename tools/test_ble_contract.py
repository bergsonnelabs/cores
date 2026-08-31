#!/usr/bin/env python3
"""Unit tests for the BLE contract binding parser.

`tests/ble-contract` covers the happy path end to end: CI builds it, so the
generated publisher has to compile and link. These tests cover what a build
cannot reach, which is every way a contract can be wrong. A rejected binding
must say what to do instead, so each case asserts on the guidance and not just
on the failure.

Run:
    python3 tools/test_ble_contract.py
"""

import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "coregen"))
from coregen import _ble_binding, _ble_ident, build_ble_contract  # noqa: E402


def bind(ch, c_type="uint8_t", is_string=False, notify=False):
    """Parse one characteristic's binding. Returns (result, errors)."""
    errors = []
    return _ble_binding(ch, "Ch", c_type, is_string, notify, errors), errors


def contract(chars, **kw):
    """Run a whole contract through the builder. Returns (ctx, errors, warnings)."""
    errors, warnings = [], []
    ctx = build_ble_contract(
        {"ble": {"contract": [{"name": "Svc", "id": "0xFE00", "characteristics": chars}]}},
        kw.get("config_path"), errors, warnings)
    return ctx, errors, warnings


class TestIdent(unittest.TestCase):
    def test_folds_to_c_identifier(self):
        self.assertEqual(_ble_ident("Power Status"), "power_status")
        self.assertEqual(_ble_ident("Battery Level"), "battery_level")

    def test_non_ascii_letters_are_separators(self):
        # str.isalnum() is true for these; a C identifier is not.
        self.assertEqual(_ble_ident("Cafe\u0301"), "cafe")   # e + combining acute
        self.assertEqual(_ble_ident("Caf\u00e9"), "caf")     # precomposed e-acute
        self.assertEqual(_ble_ident("\u6e29\u5ea6"), "unnamed")  # no ASCII at all

    def test_result_is_always_a_valid_identifier(self):
        for name in ["Caf\u00e9", "\u6e29\u5ea6",
                     "---", "9 Lives", "a  b", "3", "_"]:
            self.assertRegex(_ble_ident(name), r"^[A-Za-z_][A-Za-z0-9_]*$", name)


class TestEscapeToC(unittest.TestCase):
    def test_absent_source_is_unbound(self):
        got, errs = bind({"name": "Ch"})
        self.assertIsNone(got)
        self.assertEqual(errs, [])

    def test_explicit_code_is_unbound(self):
        got, errs = bind({"source": "code"})
        self.assertIsNone(got)
        self.assertEqual(errs, [])


class TestVarBinding(unittest.TestCase):
    def test_scalar_defaults(self):
        got, errs = bind({"source": {"var": "count"}})
        self.assertEqual(errs, [])
        self.assertEqual(got["var"], "count")
        self.assertEqual(got["storage"], "int")
        self.assertEqual(got["mode"], "on_change")
        self.assertEqual(got["hz"], 10)
        self.assertEqual(got["interval_ms"], 100)

    def test_string_storage(self):
        got, _ = bind({"source": {"var": "label"}}, c_type=None, is_string=True)
        self.assertEqual(got["storage"], "const char *")

    def test_notify_gates_on_subscription(self):
        got, _ = bind({"source": {"var": "c"}}, notify=True)
        self.assertEqual(got["gate"], "subscribed")

    def test_read_only_gates_on_connection(self):
        # No CCCD exists without notify, so there is no subscription to wait for.
        got, _ = bind({"source": {"var": "c"}}, notify=False)
        self.assertEqual(got["gate"], "connected")

    def test_publish_always_skips_change_detection(self):
        got, errs = bind({"source": {"var": "c"}, "publish": "always"})
        self.assertEqual(errs, [])
        self.assertEqual(got["mode"], "always")

    def test_publish_hz_sets_the_interval(self):
        got, _ = bind({"source": {"var": "c"}, "publish": {"hz": 4}})
        self.assertEqual(got["interval_ms"], 250)
        self.assertEqual(got["mode"], "on_change")


class TestRejections(unittest.TestCase):
    def assertRejects(self, ch, must_mention, **kw):
        got, errs = bind(ch, **kw)
        self.assertIsNone(got)
        self.assertEqual(len(errs), 1, errs)
        for token in must_mention:
            self.assertIn(token, errs[0])

    def test_tile_source_names_the_workaround(self):
        self.assertRejects({"source": {"tile": "sense_t_c.read_temp"}},
                           ["cannot emit yet", '"var"'])

    def test_bytes_cannot_bind_and_names_the_setter(self):
        self.assertRejects({"source": {"var": "buf"}}, ["bytes", "ble_ch_set()"],
                           c_type=None, is_string=False)

    def test_var_must_be_a_c_identifier(self):
        self.assertRejects({"source": {"var": "2bad"}}, ["not a C identifier"])

    def test_source_object_needs_a_var(self):
        self.assertRejects({"source": {}}, ["no 'var'"])

    def test_unknown_source_form_lists_the_valid_ones(self):
        self.assertRejects({"source": "magic"}, ['"code"', '"var"', '"tile"'])

    def test_unknown_publish_form_lists_the_valid_ones(self):
        self.assertRejects({"source": {"var": "c"}, "publish": "sometimes"},
                           ['"on_change"', '"always"', '"hz"'])

    def test_hz_must_be_a_sane_whole_number(self):
        for bad in [0, -1, 1001, 2.5, True, "10"]:
            with self.subTest(hz=bad):
                self.assertRejects({"source": {"var": "c"}, "publish": {"hz": bad}},
                                   ["1 to 1000"])


class TestDiagnostics(unittest.TestCase):
    READ = {"name": "Count", "id": "0xFE01", "access": ["read"], "type": "uint16"}

    def test_unsourced_readable_warns(self):
        _, errs, warns = contract([dict(self.READ)])
        self.assertEqual(errs, [])
        self.assertEqual(len(warns), 1)
        self.assertIn("nothing publishes 'Count'", warns[0])

    def test_explicit_code_does_not_warn(self):
        # The author said they publish it themselves. Every hand-written
        # contract does, so warning here would make the check unusable.
        _, _, warns = contract([dict(self.READ, source="code")])
        self.assertEqual(warns, [])

    def test_bound_does_not_warn(self):
        _, _, warns = contract([dict(self.READ, source={"var": "count"})])
        self.assertEqual(warns, [])

    def test_write_only_does_not_warn(self):
        # Nothing is meant to publish it; the central supplies the value.
        _, _, warns = contract([{"name": "Cmd", "id": "0xFE02",
                                 "access": ["write"], "type": "uint16"}])
        self.assertEqual(warns, [])

    def test_bound_chars_are_exposed_separately(self):
        ctx, _, _ = contract([dict(self.READ, source={"var": "count"}),
                              dict(self.READ, name="Other", id="0xFE02", source="code")])
        self.assertEqual([c["name"] for c in ctx["bound"]], ["Count"])
        self.assertEqual(len(ctx["chars"]), 2)


class TestOrphanHandlers(unittest.TestCase):
    def _project(self, main_c, chars):
        import tempfile, os
        d = tempfile.mkdtemp()
        Path(d, "main.c").write_text(main_c, encoding="utf-8")
        cfg = os.path.join(d, "config.json")
        Path(cfg).write_text("{}", encoding="utf-8")
        return contract(chars, config_path=cfg)

    CH = {"name": "Brightness", "id": "0xFE01",
          "access": ["read", "write"], "type": "uint16", "source": "code"}

    def test_stale_handler_warns(self):
        _, _, warns = self._project(
            "void ble_bright_on_write(int v) { (void)v; }\n", [dict(self.CH)])
        self.assertEqual(len(warns), 1)
        self.assertIn("ble_bright_on_write", warns[0])

    def test_matching_handler_is_quiet(self):
        _, _, warns = self._project(
            "void ble_brightness_on_write(int v) { (void)v; }\n", [dict(self.CH)])
        self.assertEqual(warns, [])

    def test_each_stale_handler_reported_once(self):
        _, _, warns = self._project(
            "void ble_bright_on_write(int v);\n"
            "void ble_bright_on_write(int v) { (void)v; }\n", [dict(self.CH)])
        self.assertEqual(len(warns), 1)


if __name__ == "__main__":
    unittest.main()
