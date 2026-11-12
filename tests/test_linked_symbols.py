import struct
import unittest
from types import SimpleNamespace

from scripts.kf.data_match import _proven_issue, linked_common_proof
from scripts.kf.linked_symbols import read_sym


def _record(address: int, kind: int, payload: bytes = b"") -> bytes:
    return struct.pack("<IB", address, kind) + payload


def _name(text: str) -> bytes:
    return bytes([len(text)]) + text.encode("latin-1")


def _claim(symbol: str, va: int, size: int):
    return SimpleNamespace(symbol=symbol, va=va, size=size,
                           section_name=".bss", scope="global")


def _unit(*claims):
    return SimpleNamespace(data=list(claims))


def _recon(*requests):
    return SimpleNamespace(common_requests=[
        SimpleNamespace(name=name, size=size) for name, size in requests])


class ReadSymTests(unittest.TestCase):
    def test_reads_symbols_across_debug_records(self) -> None:
        data = b"MND\x01\x00\x00\x00\x00"
        data += _record(0x80011000, 1, _name("main"))
        data += _record(0, 0x88, struct.pack("<I", 7) + _name("MAIN.C"))
        data += _record(0x80011000, 0x8C, bytes(20) + _name("C:\\MAIN.C") + _name("main"))
        data += _record(0, 0x96, bytes(8) + struct.pack("<H", 1) + bytes(4)
                        + _name("TAG") + _name("x"))
        data += _record(0x800A1EF4, 2, _name("disp_db"))
        self.assertEqual(read_sym(data), {"main": 0x80011000, "disp_db": 0x800A1EF4})

    def test_rejects_unknown_records(self) -> None:
        with self.assertRaisesRegex(ValueError, "unknown SYM record type 0x6e"):
            read_sym(b"MND\x01\x00\x00\x00\x00" + _record(0, 0x6E))


class LinkedCommonProofTests(unittest.TestCase):
    def test_every_request_at_its_claim_is_proven(self) -> None:
        unit = _unit(_claim("a", 0x100, 4), _claim("b", 0x200, 8))
        proof = linked_common_proof(unit, _recon(("a", 8), ("b", 8)),
                                    {"a": 0x100, "b": 0x200})
        self.assertEqual(proof, {"a", "b"})

    def test_one_misplaced_request_proves_nothing(self) -> None:
        unit = _unit(_claim("a", 0x100, 4), _claim("b", 0x200, 8))
        proof = linked_common_proof(unit, _recon(("a", 8), ("b", 8)),
                                    {"a": 0x100, "b": 0x208})
        self.assertEqual(proof, frozenset())

    def test_unclaimed_or_short_request_or_stale_link_proves_nothing(self) -> None:
        unit = _unit(_claim("a", 0x100, 8))
        self.assertFalse(linked_common_proof(unit, _recon(("c", 8)), {"c": 0x100}))
        self.assertFalse(linked_common_proof(unit, _recon(("a", 4)), {"a": 0x100}))
        self.assertFalse(linked_common_proof(unit, _recon(("a", 8)), None))

    def test_only_common_restatements_are_waived(self) -> None:
        proven = frozenset({"a"})
        self.assertTrue(_proven_issue("reconstruction", {
            "kind": "unresolved-common-placement", "symbol": "a"}, proven))
        self.assertTrue(_proven_issue("reconstruction", {
            "kind": "owned-symbol-layout", "symbol": "a", "actual_section": "SHN_COMMON",
            "expected_section": ".bss", "actual_size": 8, "expected_size": 4}, proven))
        self.assertTrue(_proven_issue("target", {
            "kind": "conflicting-section-bases", "section": ".bss",
            "claims": [{"symbol": "a"}]}, proven))
        self.assertFalse(_proven_issue("target", {
            "kind": "conflicting-section-bases", "section": ".bss",
            "claims": [{"symbol": "a"}, {"symbol": "b"}]}, proven))
        self.assertFalse(_proven_issue("reconstruction", {
            "kind": "owned-symbol-layout", "symbol": "a", "actual_section": ".data",
            "expected_section": ".bss", "actual_size": 8, "expected_size": 4}, proven))
        self.assertFalse(_proven_issue("reconstruction", {
            "kind": "unresolved-common-placement", "symbol": "b"}, proven))


if __name__ == "__main__":
    unittest.main()
