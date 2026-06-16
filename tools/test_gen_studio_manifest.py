#!/usr/bin/env python3
"""Unit tests for the @studio annotation parser.

Focused on the enum-label extension landed for Pre-A4 — the rest of
gen_studio_manifest.py is exercised end-to-end by `--check` against
the shipped manifests, so these tests cover just the path that can't
be validated that way (no driver uses enum labels yet; the tests are
the enforcement until one does).

Run:
    python3 tools/test_gen_studio_manifest.py
    # or
    python3 -m unittest tools.test_gen_studio_manifest
"""

import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path

# Make the generator module importable whether this file is run as a
# script or via unittest discovery.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_studio_manifest import (  # noqa: E402
    build_host_entry,
    load_bus_addresses,
    parse_enum_body,
    parse_layer_docs,
    parse_studio_tags,
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


class ParseStudioTagsEnum(unittest.TestCase):
    def test_param_enum_tag(self):
        lines = [
            "@studio expose category=tile name=set_range",
            "@studio param range enum {FSR_2G=2g, FSR_4G=4g, FSR_8G=8g}",
        ]
        tags = parse_studio_tags(lines)
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
        lines = ["@studio param mode enum {A=a, B=b, C=c}"]
        tags = parse_studio_tags(lines)
        self.assertEqual(len(tags[0][2]["enum"]), 3)

    def test_other_attrs_still_parsed(self):
        lines = [
            "@studio expose category=tile name=foo icon=★",
        ]
        tags = parse_studio_tags(lines)
        _, _, attrs = tags[0]
        self.assertEqual(attrs["category"], "tile")
        self.assertEqual(attrs["name"], "foo")
        self.assertEqual(attrs["icon"], "★")

    def test_no_braces_no_enum(self):
        lines = ["@studio param range"]
        tags = parse_studio_tags(lines)
        _, positional, attrs = tags[0]
        self.assertEqual(positional, "range")
        self.assertNotIn("enum", attrs)


class BuildHostEntryEnum(unittest.TestCase):
    """Integration: a studio-exposed function with an enum param emits
    the enum labels in its dsl_params entry."""

    def _build(self, studio_param_attrs):
        doxy_lines = [
            "@brief Set accelerometer range.",
            "@studio expose category=tile name=set_accel_range",
            "@param range Full-scale range setting.",
            f"@studio param range enum {{{studio_param_attrs}}}",
        ]
        tags = parse_studio_tags(doxy_lines)
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
            "@studio expose category=tile name=set_accel_range",
            "@param range Full-scale range setting.",
        ]
        tags = parse_studio_tags(doxy_lines)
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
    """`@studio out_buffer <cname> type=... length=...` strips the
    param from the DSL-facing list and emits `c_out_buffer` on the host.
    """

    def _build(self, extra_lines=None):
        doxy_lines = [
            "@brief Read raw accelerometer [X, Y, Z].",
            "@studio expose category=tile name=get_raw_accels returns=int[3]",
            "@studio out_buffer buffer type=int16_t length=3",
            "@param buffer Caller-owned buffer receiving the 3 axes.",
        ]
        if extra_lines:
            doxy_lines.extend(extra_lines)
        tags = parse_studio_tags(doxy_lines)
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
        # Without @studio out_buffer, c_out_buffer must be absent even
        # on void-returning hosts. Gatekeeps against a future default
        # that would emit it unconditionally.
        doxy_lines = [
            "@studio expose category=tile name=sleep",
        ]
        tags = parse_studio_tags(doxy_lines)
        expose = next((a for v, _, a in tags if v == "expose"), None)
        sig = {
            "returns": "void",
            "name": "tile_x_sleep",
            "params": [{"name": "tile", "ctype": "tile_t *"}],
        }
        host = build_host_entry(expose, doxy_lines, sig, "x.h", scope="tile", all_tags=tags)
        self.assertNotIn("c_out_buffer", host)


class BuildHostEntryArrayInParam(unittest.TestCase):
    """`@studio param <cname> type=int[N]` overrides the DSL type for
    a pointer-typed C param so it appears as a fixed-length array in
    the manifest."""

    def test_array_in_param_type_override(self):
        doxy_lines = [
            "@studio expose category=tile name=play_sequence",
            "@studio param effects type=int[16]",
            "@param effects Array of effect indices.",
            "@param count Number of effects.",
        ]
        tags = parse_studio_tags(doxy_lines)
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
    """When @studio out_buffer strips a param from the middle of the C
    signature, remaining dsl_params still pick up the correct @param
    metadata — aligned by name, not position."""

    def test_out_buffer_doesnt_misalign_neighbour(self):
        doxy_lines = [
            "@studio expose category=tile name=sample_many returns=int[3]",
            "@studio out_buffer buffer type=int16_t length=3",
            "@param buffer Caller-owned buffer.",
            "@param channel [0..3] ADC channel to sample.",
        ]
        tags = parse_studio_tags(doxy_lines)
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


class LoadBusAddresses(unittest.TestCase):
    """`load_bus_addresses` reads a tile-definition JSON and
    surfaces `interfaces[].parameters.addresses` onto the manifest,
    keyed by bus name."""

    def _write_def(self, payload):
        tmp = Path(tempfile.mkstemp(suffix=".json")[1])
        tmp.write_text(json.dumps(payload))
        self.addCleanup(tmp.unlink)
        return tmp

    def test_none_path_returns_empty(self):
        self.assertEqual(load_bus_addresses(None), {})

    def test_missing_file_returns_empty(self):
        missing = Path("/tmp/does-not-exist-studio-test.json")
        self.assertEqual(load_bus_addresses(missing), {})

    def test_invalid_json_returns_empty(self):
        tmp = Path(tempfile.mkstemp(suffix=".json")[1])
        tmp.write_text("{not valid")
        self.addCleanup(tmp.unlink)
        self.assertEqual(load_bus_addresses(tmp), {})

    def test_extracts_i2c_addresses_with_default(self):
        tmp = self._write_def({
            "interfaces": [
                {
                    "name": "I2C",
                    "parameters": {
                        "addresses": [
                            {"address": "0x69", "is_default": True},
                            {"address": "0x68"},
                        ],
                    },
                }
            ]
        })
        self.assertEqual(
            load_bus_addresses(tmp),
            {"I2C": [
                {"address": "0x69", "is_default": True},
                {"address": "0x68"},
            ]},
        )

    def test_strips_extra_fields_from_address_entries(self):
        # Schema-growth safety: if the tile-def format adds fields per
        # address (e.g., a future `pad_select`), we don't leak them
        # into the manifest until we explicitly surface them. Keeps
        # the manifest shape stable across tile-def schema bumps.
        tmp = self._write_def({
            "interfaces": [
                {
                    "name": "I2C",
                    "parameters": {
                        "addresses": [
                            {"address": "0x50", "future_field": "ignored"},
                        ],
                    },
                }
            ]
        })
        self.assertEqual(
            load_bus_addresses(tmp),
            {"I2C": [{"address": "0x50"}]},
        )

    def test_skips_interfaces_without_addresses(self):
        # I3C (dynamic address assignment) and SPI (CS-based) typically
        # carry no `addresses` list. Omitting them from the output lets
        # the frontend treat "bus key present" as a positive signal
        # that an address variant is selectable for this bus.
        tmp = self._write_def({
            "interfaces": [
                {"name": "I3C", "parameters": {"max_clock_speed": "12.5MHz"}},
                {"name": "SPI", "parameters": {}},
                {
                    "name": "I2C",
                    "parameters": {"addresses": [{"address": "0x42"}]},
                },
            ]
        })
        result = load_bus_addresses(tmp)
        self.assertIn("I2C", result)
        self.assertNotIn("I3C", result)
        self.assertNotIn("SPI", result)

    def test_empty_interfaces_list_returns_empty(self):
        tmp = self._write_def({"interfaces": []})
        self.assertEqual(load_bus_addresses(tmp), {})

    def test_no_interfaces_key_returns_empty(self):
        tmp = self._write_def({"name": "x"})
        self.assertEqual(load_bus_addresses(tmp), {})


class ParseLayerDocs(unittest.TestCase):
    """The hal_/ll_ layer-docs path: dedup of #if variants, curation +
    ordering via `only`, layer tagging, and a warning for missing names."""

    HEADER = """\
#if defined(STM32L011xx)
/** Enable the oscillator. */
static inline void ll_x_enable(void) { }
#elif defined(STM32H523xx)
static inline void ll_x_enable(void) { }
#endif

/** Read the thing. */
static inline uint32_t ll_x_read(uint32_t mask) { return mask; }

/** Not in the curated list. */
static inline void ll_x_extra(void) { }
"""

    def _write(self):
        tmp = Path(tempfile.mkstemp(suffix=".h")[1])
        tmp.write_text(self.HEADER)
        return tmp

    def test_dedup_and_layer_tag(self):
        out = parse_layer_docs(self._write(), "ll")
        # the family-gated #if variant collapses to one entry, source order
        self.assertEqual(
            [f["name"] for f in out], ["ll_x_enable", "ll_x_read", "ll_x_extra"]
        )
        self.assertTrue(all(f["layer"] == "ll" for f in out))
        self.assertEqual(out[0]["brief"], "Enable the oscillator.")

    def test_only_curates_and_orders(self):
        out = parse_layer_docs(self._write(), "ll", only=["ll_x_read", "ll_x_enable"])
        self.assertEqual([f["name"] for f in out], ["ll_x_read", "ll_x_enable"])

    def test_only_warns_on_missing(self):
        err = io.StringIO()
        with redirect_stderr(err):
            out = parse_layer_docs(self._write(), "ll", only=["ll_x_read", "ll_x_nope"])
        self.assertEqual([f["name"] for f in out], ["ll_x_read"])
        self.assertIn("ll_x_nope", err.getvalue())


if __name__ == "__main__":
    unittest.main()
