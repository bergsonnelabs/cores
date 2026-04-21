#!/usr/bin/env python3
"""Unit tests for the @tessera annotation parser.

Focused on the enum-label extension landed for Pre-A4 — the rest of
gen_tessera_manifest.py is exercised end-to-end by `--check` against
the shipped manifests, so these tests cover just the path that can't
be validated that way (no driver uses enum labels yet; the tests are
the enforcement until one does).

Run:
    python3 tools/test_gen_tessera_manifest.py
    # or
    python3 -m unittest tools.test_gen_tessera_manifest
"""

import sys
import unittest
from pathlib import Path

# Make the generator module importable whether this file is run as a
# script or via unittest discovery.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_tessera_manifest import (  # noqa: E402
    build_host_entry,
    parse_enum_body,
    parse_tessera_tags,
)


class ParseEnumBody(unittest.TestCase):
    def test_simple(self):
        self.assertEqual(
            parse_enum_body("A=alpha, B=beta"),
            [
                {"c_name": "A", "label": "alpha"},
                {"c_name": "B", "label": "beta"},
            ],
        )

    def test_strips_whitespace(self):
        self.assertEqual(
            parse_enum_body("  A  =  alpha  ,  B=beta"),
            [
                {"c_name": "A", "label": "alpha"},
                {"c_name": "B", "label": "beta"},
            ],
        )

    def test_trailing_comma_ignored(self):
        self.assertEqual(
            parse_enum_body("A=alpha, B=beta,"),
            [
                {"c_name": "A", "label": "alpha"},
                {"c_name": "B", "label": "beta"},
            ],
        )

    def test_missing_label_uses_cname(self):
        self.assertEqual(
            parse_enum_body("A, B"),
            [
                {"c_name": "A", "label": "A"},
                {"c_name": "B", "label": "B"},
            ],
        )

    def test_label_can_contain_spaces_and_symbols(self):
        # Labels are display strings — everything up to the next comma
        # is the label verbatim.
        self.assertEqual(
            parse_enum_body("FSR_4G=±4 g, FSR_8G=±8 g"),
            [
                {"c_name": "FSR_4G", "label": "±4 g"},
                {"c_name": "FSR_8G", "label": "±8 g"},
            ],
        )


class ParseTesseraTagsEnum(unittest.TestCase):
    def test_param_enum_tag(self):
        lines = [
            "@tessera expose category=tile name=set_range",
            "@tessera param range enum {FSR_2G=2g, FSR_4G=4g, FSR_8G=8g}",
        ]
        tags = parse_tessera_tags(lines)
        self.assertEqual(len(tags), 2)
        expose = tags[0]
        self.assertEqual(expose[0], "expose")
        param_tag = tags[1]
        self.assertEqual(param_tag[0], "param")
        self.assertEqual(param_tag[1], "range")  # positional = C param name
        self.assertIn("enum", param_tag[2])
        self.assertEqual(
            param_tag[2]["enum"],
            [
                {"c_name": "FSR_2G", "label": "2g"},
                {"c_name": "FSR_4G", "label": "4g"},
                {"c_name": "FSR_8G", "label": "8g"},
            ],
        )

    def test_comma_inside_braces_survives(self):
        # Before the brace-aware extraction, rest.split() would have
        # shredded the enum body at every comma.
        lines = ["@tessera param mode enum {A=a, B=b, C=c}"]
        tags = parse_tessera_tags(lines)
        self.assertEqual(len(tags[0][2]["enum"]), 3)

    def test_other_attrs_still_parsed(self):
        lines = [
            "@tessera expose category=tile name=foo icon=★",
        ]
        tags = parse_tessera_tags(lines)
        _, _, attrs = tags[0]
        self.assertEqual(attrs["category"], "tile")
        self.assertEqual(attrs["name"], "foo")
        self.assertEqual(attrs["icon"], "★")

    def test_no_braces_no_enum(self):
        lines = ["@tessera param range"]
        tags = parse_tessera_tags(lines)
        _, positional, attrs = tags[0]
        self.assertEqual(positional, "range")
        self.assertNotIn("enum", attrs)


class BuildHostEntryEnum(unittest.TestCase):
    """Integration: a tessera-exposed function with an enum param emits
    the enum labels in its dsl_params entry."""

    def _build(self, tessera_param_attrs):
        doxy_lines = [
            "@brief Set accelerometer range.",
            "@tessera expose category=tile name=set_accel_range",
            "@param range Full-scale range setting.",
            f"@tessera param range enum {{{tessera_param_attrs}}}",
        ]
        tags = parse_tessera_tags(doxy_lines)
        expose = next((a for v, _, a in tags if v == "expose"), None)
        sig = {
            "returns": "void",
            "name": "tile_sense_i_6p6_set_accel_range",
            "params": [
                {"name": "tile", "ctype": "tile_t *"},
                {"name": "range", "ctype": "sense_i_6p6_accel_range_t"},
            ],
        }
        return build_host_entry(
            expose,
            doxy_lines,
            sig,
            "tile_sense_i_6p6.h",
            scope="tile",
            all_tags=tags,
        )

    def test_enum_attached_to_param(self):
        host = self._build("FSR_2G=2g, FSR_4G=4g, FSR_8G=8g")
        params = host["params"]
        self.assertEqual(len(params), 1)
        p = params[0]
        self.assertEqual(p["name"], "range")
        self.assertEqual(len(p["enum"]), 3)
        self.assertEqual(p["enum"][0], {"c_name": "FSR_2G", "label": "2g"})

    def test_no_enum_when_not_annotated(self):
        doxy_lines = [
            "@tessera expose category=tile name=set_accel_range",
            "@param range Full-scale range setting.",
        ]
        tags = parse_tessera_tags(doxy_lines)
        expose = next((a for v, _, a in tags if v == "expose"), None)
        sig = {
            "returns": "void",
            "name": "x",
            "params": [
                {"name": "tile", "ctype": "tile_t *"},
                {"name": "range", "ctype": "sense_i_6p6_accel_range_t"},
            ],
        }
        host = build_host_entry(expose, doxy_lines, sig, "x.h", scope="tile", all_tags=tags)
        self.assertNotIn("enum", host["params"][0])


class BuildHostEntryOutBuffer(unittest.TestCase):
    """`@tessera out_buffer <cname> type=... length=...` strips the
    param from the DSL-facing list and emits `c_out_buffer` on the host.
    """

    def _build(self, extra_lines=None):
        doxy_lines = [
            "@brief Read raw accelerometer [X, Y, Z].",
            "@tessera expose category=tile name=get_raw_accels returns=int[3]",
            "@tessera out_buffer buffer type=int16_t length=3",
            "@param buffer Caller-owned buffer receiving the 3 axes.",
        ]
        if extra_lines:
            doxy_lines.extend(extra_lines)
        tags = parse_tessera_tags(doxy_lines)
        expose = next((a for v, _, a in tags if v == "expose"), None)
        sig = {
            "returns": "void",
            "name": "tile_sense_i_6p6_get_raw_accels",
            "params": [
                {"name": "tile", "ctype": "tile_t *"},
                {"name": "buffer", "ctype": "int16_t *"},
            ],
        }
        return build_host_entry(
            expose, doxy_lines, sig, "tile_sense_i_6p6.h",
            scope="tile", all_tags=tags,
        )

    def test_out_buffer_stripped_from_params(self):
        host = self._build()
        # Buffer param should not appear in DSL-facing params.
        param_names = [p["name"] for p in host["params"]]
        self.assertNotIn("buffer", param_names)

    def test_c_out_buffer_metadata_emitted(self):
        host = self._build()
        self.assertEqual(
            host["c_out_buffer"],
            {"type": "int16_t", "length": 3},
        )

    def test_dsl_returns_carries_array_type(self):
        host = self._build()
        self.assertEqual(host["dsl_returns"], "int[3]")

    def test_no_out_buffer_no_field(self):
        # Without @tessera out_buffer, c_out_buffer must be absent even
        # on void-returning hosts. Gatekeeps against a future default
        # that would emit it unconditionally.
        doxy_lines = [
            "@tessera expose category=tile name=sleep",
        ]
        tags = parse_tessera_tags(doxy_lines)
        expose = next((a for v, _, a in tags if v == "expose"), None)
        sig = {
            "returns": "void",
            "name": "tile_x_sleep",
            "params": [{"name": "tile", "ctype": "tile_t *"}],
        }
        host = build_host_entry(expose, doxy_lines, sig, "x.h", scope="tile", all_tags=tags)
        self.assertNotIn("c_out_buffer", host)


class BuildHostEntryArrayInParam(unittest.TestCase):
    """`@tessera param <cname> type=int[N]` overrides the DSL type for
    a pointer-typed C param so it appears as a fixed-length array in
    the manifest."""

    def test_array_in_param_type_override(self):
        doxy_lines = [
            "@tessera expose category=tile name=play_sequence",
            "@tessera param effects type=int[16]",
            "@param effects Array of effect indices.",
            "@param count Number of effects.",
        ]
        tags = parse_tessera_tags(doxy_lines)
        expose = next((a for v, _, a in tags if v == "expose"), None)
        sig = {
            "returns": "void",
            "name": "tile_drive_h_play_sequence",
            "params": [
                {"name": "tile", "ctype": "tile_t *"},
                {"name": "effects", "ctype": "const uint8_t *"},
                {"name": "count", "ctype": "uint8_t"},
            ],
        }
        host = build_host_entry(
            expose, doxy_lines, sig, "tile_drive_h.h",
            scope="tile", all_tags=tags,
        )
        by_name = {p["name"]: p for p in host["params"]}
        self.assertEqual(by_name["effects"]["type"], "int[16]")
        # count stays int (no override)
        self.assertEqual(by_name["count"]["type"], "int")


class DoxyParamNameAlignmentAfterStripping(unittest.TestCase):
    """When @tessera out_buffer strips a param from the middle of the C
    signature, remaining dsl_params still pick up the correct @param
    metadata — aligned by name, not position."""

    def test_out_buffer_doesnt_misalign_neighbour(self):
        doxy_lines = [
            "@tessera expose category=tile name=sample_many returns=int[3]",
            "@tessera out_buffer buffer type=int16_t length=3",
            "@param buffer Caller-owned buffer.",
            "@param channel [0..3] ADC channel to sample.",
        ]
        tags = parse_tessera_tags(doxy_lines)
        expose = next((a for v, _, a in tags if v == "expose"), None)
        sig = {
            "returns": "void",
            "name": "tile_x_sample_many",
            "params": [
                {"name": "tile", "ctype": "tile_t *"},
                {"name": "buffer", "ctype": "int16_t *"},
                {"name": "channel", "ctype": "uint8_t"},
            ],
        }
        host = build_host_entry(
            expose, doxy_lines, sig, "x.h", scope="tile", all_tags=tags,
        )
        by_name = {p["name"]: p for p in host["params"]}
        # channel should carry its own @param metadata, not buffer's.
        # Without by-name alignment this would land the buffer's empty
        # meta on channel and drop the [0..3] range.
        self.assertEqual(by_name["channel"].get("range"), [0, 3])
        self.assertIn("ADC channel", by_name["channel"].get("description", ""))


if __name__ == "__main__":
    unittest.main()
