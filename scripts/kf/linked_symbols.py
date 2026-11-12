"""Symbol addresses PSYLINK assigned in the current native executable link.

PSYLINK alone places exported COMMON requests (by symbol-hash bucket), so an
object file cannot show where a tentative global lands. The linker's SN
Systems ``.SYM`` output can. ``linked_addresses`` returns its symbol table
only when ``build/link/<image>/build.json`` records a completed link whose
every compiled input still hashes to the current file, so a stale link never
vouches for edited sources.
"""

from __future__ import annotations

import hashlib
import json
import struct
from pathlib import Path

from scripts.kf.paths import BUILD, REPO

LINK_DIR = BUILD / "link"

# SN MND record types and the fixed payload bytes before any name strings.
# 0x8c (function start) carries a file name and a function name; 0x96
# (definition with dimensions) carries dims words, a tag and a name.
_FIXED = {0x80: 0, 0x82: 1, 0x84: 2, 0x86: 4, 0x8a: 0, 0x8e: 4, 0x90: 4,
          0x92: 4, 0x98: 8, 0x9a: 0}
_FIXED_THEN_NAMES = {0x88: (4, 1), 0x8c: (20, 2), 0x94: (8, 1)}


def read_sym(data: bytes) -> dict[str, int]:
    """Labels and globals (record types 1 and 2) of an SN ``MND`` symbol file."""
    if data[:3] != b"MND":
        raise ValueError("not an SN MND symbol file")
    symbols: dict[str, int] = {}
    pos = 8

    def string(at: int) -> tuple[str, int]:
        if at >= len(data) or at + 1 + data[at] > len(data):
            raise ValueError(f"truncated symbol string at {at:#x}")
        length = data[at]
        return data[at + 1:at + 1 + length].decode("latin-1"), at + 1 + length

    while pos < len(data):
        if pos + 5 > len(data):
            raise ValueError(f"truncated record at {pos:#x}")
        address, kind = struct.unpack_from("<IB", data, pos)
        pos += 5
        if kind in (1, 2):
            name, pos = string(pos)
            symbols[name] = address
        elif kind in _FIXED:
            pos += _FIXED[kind]
        elif kind in _FIXED_THEN_NAMES:
            fixed, names = _FIXED_THEN_NAMES[kind]
            pos += fixed
            for _ in range(names):
                _, pos = string(pos)
        elif kind == 0x96:
            pos += 8
            if pos + 2 > len(data):
                raise ValueError(f"truncated definition at {pos:#x}")
            dims = struct.unpack_from("<H", data, pos)[0]
            pos += 2 + 4 * dims
            _, pos = string(pos)
            _, pos = string(pos)
        else:
            raise ValueError(f"unknown SYM record type {kind:#x} at {pos - 5:#x}")
    return symbols


def _file_sha256(path: Path) -> str | None:
    try:
        return hashlib.sha256(path.read_bytes()).hexdigest()
    except OSError:
        return None


def linked_addresses(image: str, link_dir: Path = LINK_DIR) -> dict[str, int] | None:
    """Symbol addresses of IMAGE's current link, or None if absent or stale."""
    key = image.removesuffix(".EXE").lower()
    report_path = link_dir / key / "build.json"
    sym_path = link_dir / key / f"{key.upper()}.SYM"
    try:
        report = json.loads(report_path.read_text())
        data = sym_path.read_bytes()
    except (OSError, ValueError):
        return None
    if not report.get("linked") or report.get("phase") != "complete":
        return None
    for unit in report.get("units", ()):
        for relative, digest in unit.get("source_sha256", {}).items():
            if _file_sha256(REPO / relative) != digest:
                return None
    return read_sym(data)
