"""Tests for the source readability ratchet (scripts.kf.readability)."""

from __future__ import annotations

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest import mock

from scripts.kf import readability
from scripts.kf.readability import Source

SAMPLE = '''\
#define KF_THING_7 7
#define KF_SCALE 0x1000
KF_ENUM_BEGIN(KfKind, u8)
    KF_KIND_0 = 0,
    KF_KIND_1,
    KF_KIND_FIRE = 2,
    KF_KIND_3,
    KF_QUARTER_TURN_1 = 1,
KF_ENUM_END(KfKind)

typedef union KfThingWord0c {
    u16 value;
    u8 word_0c;
} KfThingWord0c;
typedef char kf_thing_size[sizeof(KfThingWord0c) == 2 ? 1 : -1];

static const u8 table[4] = { 0x10, 0x20, 0x30, 0x40 };

/* written by 0x80012345; objdiff 98.5% */
ADDRESS(0x80010000, 0x40)
s32 thing_update_1(KfThing *thing, s32 arg3)
{
    const char *s = "0xdead goto x";
    switch (thing->state) {
    case 3:
        thing->phase = 2;
        thing->yaw_step = 0;
        thing->animation_phase = 0;
        thing->step = 1;
        if (thing->flags & 0x40) {
            goto done;
        }
        break;
    }
    thing->word.value = thing->word_0c & 0xff;
    codes[0] = 1;
    codes[1] = 2;
    codes[2] = 3;
    codes[3] = 0x14;
done:
    return thing->speed + 0x200;
}
'''


def scan_sample() -> dict[str, list[readability.Finding]]:
    with mock.patch.object(readability, "load_retained_gotos", return_value=set()):
        return readability.scan([Source("src/game/sample.c", SAMPLE)])


def labels(results: dict[str, list[readability.Finding]], label: str) -> list[str]:
    return [f.text for f in results[label]]


class SourceViewTest(unittest.TestCase):
    def test_views_keep_offsets_and_blank_comments_and_strings(self) -> None:
        source = Source("x.c", SAMPLE)
        self.assertEqual(len(source.code), len(SAMPLE))
        self.assertEqual(len(source.body), len(SAMPLE))
        self.assertNotIn("0xdead", source.code)
        self.assertNotIn("objdiff", source.code)
        self.assertNotIn("0x10, 0x20", source.body)  # initializer
        self.assertNotIn("0x1000", source.body)  # #define

    def test_function_spans_and_lines(self) -> None:
        source = Source("x.c", SAMPLE)
        self.assertEqual([f.name for f in source.functions], ["thing_update_1"])
        offset = SAMPLE.index("goto done")
        self.assertEqual(source.function_at(offset).name, "thing_update_1")
        self.assertEqual(source.line(offset), SAMPLE[:offset].count("\n") + 1)
        self.assertIsNone(source.function_at(SAMPLE.index("KfThingWord0c")))


class NameMetricTest(unittest.TestCase):
    def test_value_echo(self) -> None:
        self.assertTrue(readability.echoes_value("KF_EFFECT_KIND_102", 102))
        self.assertTrue(readability.echoes_value("KF_FLAG_40", 0x40))
        self.assertFalse(readability.echoes_value("KF_ITEM_RESTORE_HP_50", 23))
        self.assertFalse(readability.echoes_value("KF_KIND_FIRE", 2))
        self.assertFalse(readability.echoes_value("KF_QUARTER_TURN_2", 2))

    def test_names(self) -> None:
        results = scan_sample()
        self.assertEqual(sorted(labels(results, "value-echo constants")),
                         ["KF_KIND_0", "KF_KIND_1", "KF_KIND_3", "KF_THING_7"])
        self.assertEqual(labels(results, "offset-named members"), ["word_0c"])
        self.assertEqual(labels(results, "offset-named member uses"), ["->word_0c"])
        self.assertEqual(labels(results, "offset-named types"), ["KfThingWord0c"])
        self.assertEqual(labels(results, "numbered functions"), ["thing_update_1"])
        self.assertEqual(labels(results, "placeholder parameters"), ["arg3"])


class LiteralAndFlowMetricTest(unittest.TestCase):
    def test_literals(self) -> None:
        results = scan_sample()
        self.assertEqual(labels(results, "raw case labels"), ["case 3:"])
        self.assertEqual(labels(results, "raw state values"),
                         ["->phase = 2", "->step = 1"])  # rates and cycle positions skip
        self.assertEqual(labels(results, "raw bit masks"), ["& 0x40"])  # 0xff truncates
        self.assertEqual(labels(results, "raw hex literals"), ["0x40", "0xff", "0x14", "0x200"])
        self.assertEqual(labels(results, "literal element fills"), ["4 element stores"])
        self.assertEqual(labels(results, "union scalar views"), [".value"])

    def test_flow_and_comments(self) -> None:
        results = scan_sample()
        self.assertEqual(labels(results, "gotos"), ["goto"])
        self.assertEqual(labels(results, "unlisted gotos"), ["goto done"])
        self.assertEqual(labels(results, "addresses in comments"), ["0x80012345"])
        self.assertEqual(labels(results, "matching-history comments"), ["objdiff", "98.5%"])
        self.assertEqual(results["long functions"], [])

    def test_retained_goto_is_listed(self) -> None:
        with TemporaryDirectory() as tmp:
            inventory = Path(tmp) / "retained.tsv"
            inventory.write_text("# header\nfile\tfunction\tlabel\tgotos\n"
                                 "src/game/sample.c\tthing_update_1\tdone\t1\n")
            with mock.patch.object(readability, "RETAINED_GOTOS", inventory):
                results = readability.scan([Source("src/game/sample.c", SAMPLE)])
        self.assertEqual(results["unlisted gotos"], [])


class RatchetTest(unittest.TestCase):
    def test_gate_fails_closed_and_on_rise(self) -> None:
        rows = [("gotos", 5), ("raw shift amounts", 99)]
        self.assertEqual(readability.gate(rows, {"gotos": 5}), [])
        self.assertEqual(readability.gate(rows, {"gotos": 4}), ["gotos: 4 -> 5 (+1)"])
        self.assertEqual(readability.gate(rows, {}), ["gotos: no committed floor"])

    def test_baseline_round_trip_keeps_only_gated(self) -> None:
        with TemporaryDirectory() as tmp:
            path = Path(tmp) / "floors.tsv"
            readability.save_baseline([("gotos", 5), ("raw shift amounts", 9)], path)
            self.assertEqual(readability.load_baseline(path), {"gotos": 5})

    def test_function_debt_ranks_gated_findings(self) -> None:
        ranked = readability.function_debt(scan_sample())
        path, name, counts = ranked[0]
        self.assertEqual((path, name), ("src/game/sample.c", "thing_update_1"))
        self.assertNotIn("raw shift amounts", counts)
        self.assertEqual(counts["raw hex literals"], 4)


class LiveCountTest(unittest.TestCase):
    def test_committed_floors_hold_for_the_tree(self) -> None:
        if not readability.BASELINE.is_file():
            self.skipTest("no committed readability baseline")
        rows = readability.totals(readability.scan(readability.load_sources()))
        self.assertEqual(readability.gate(rows, readability.load_baseline()), [])


if __name__ == "__main__":
    unittest.main()
