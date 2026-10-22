"""Validate the hand-owned King's Field II retail executable censuses."""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import os
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


IMAGE_ORDER = {"PSX.EXE": 0, "GAME.EXE": 1, "OPEN.EXE": 2, "END.EXE": 3}


@dataclass(frozen=True)
class ImageLayout:
    name: str
    sha256: str
    file_size: int
    entry: int
    load_address: int
    load_size: int

    @property
    def load_end(self) -> int:
        return self.load_address + self.load_size

    def contains(self, address: int, size: int = 1) -> bool:
        return size >= 0 and self.load_address <= address and address + size <= self.load_end

    def file_offset(self, address: int) -> int:
        return 0x800 + address - self.load_address


IMAGE_LAYOUTS = {
    "PSX.EXE": ImageLayout(
        "PSX.EXE",
        "8a94d391291c413bed1aa0ed1ac0adae4328a4841058346b8af1fd41ea3e45a6",
        0x1000,
        0x80010128,
        0x80010000,
        0x800,
    ),
    "GAME.EXE": ImageLayout(
        "GAME.EXE",
        "3ca696397ace8b062bce71d390a79f5b5ca6e941ed23c0cc7adb8b261553b10a",
        0x5E800,
        0x8004A628,
        0x80011000,
        0x5E000,
    ),
    "OPEN.EXE": ImageLayout(
        "OPEN.EXE",
        "8f73f299ce1335bb23c9e438047f470dde37600ac38b558663333438a5b7b324",
        0x2E800,
        0x80013DB8,
        0x80011000,
        0x2E000,
    ),
    "END.EXE": ImageLayout(
        "END.EXE",
        "602b87f70d87f7decbb9d934b7c29c567c811632b1d4f59f88e4ce6b0a263da3",
        0x29800,
        0x80011CB4,
        0x80011000,
        0x29000,
    ),
}


FUNCTION_FIELDS = (
    "image",
    "va",
    "file_offset",
    "size",
    "body_size",
    "fragments",
    "kind",
    "confidence",
    "name",
    "labels",
    "provenance",
    "note",
)
DATA_FIELDS = (
    "image",
    "va",
    "file_offset",
    "size",
    "kind",
    "confidence",
    "name",
    "datatype",
    "provenance",
    "note",
)
RELOC_FIELDS = (
    "image",
    "site_va",
    "site_file_offset",
    "paired_site_va",
    "kind",
    "channel",
    "target_va",
    "target_region",
    "target_name",
    "opcode",
    "register",
    "confidence",
    "status",
    "provenance",
)
VENDORED_FUNCTION_FIELDS = (
    "image",
    "va",
    "size",
    "name",
    "aliases",
    "provider",
    "library",
    "module",
    "member_offset",
    "source_version",
    "evidence",
    "confidence",
    "provenance",
    "note",
)

VENDORED_CONFIDENCE = {
    "fid-sdk30",
    "fid-sdk30-ambiguous",
    "exact-sdk30",
    "exact-sdk30-short",
    "exact-sdk30-ambiguous",
    "exact-sdk30-complete",
    "psyq300-signature",
    "psyq300-signature-ambiguous",
    "sdk-lineage-supported",
}


def parse_int(value: str) -> int:
    return int(value, 0)


def format_hex(value: int) -> str:
    return f"0x{value:08x}"


def format_size(value: int) -> str:
    return f"0x{value:x}"


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def read_tsv(path: Path) -> tuple[tuple[str, ...], list[dict[str, str]]]:
    lines = [line for line in path.read_text(encoding="utf-8").splitlines()
             if line and not line.startswith("#")]
    if not lines:
        raise ValueError(f"{path}: missing TSV header")
    reader = csv.DictReader(lines, delimiter="\t")
    fields = tuple(reader.fieldnames or ())
    return fields, list(reader)


def write_tsv(
    path: Path,
    fields: Iterable[str],
    rows: Iterable[dict[str, object]],
    comments: Iterable[str],
) -> None:
    stream = io.StringIO(newline="")
    for comment in comments:
        stream.write(f"# {comment}\n")
    writer = csv.DictWriter(
        stream,
        tuple(fields),
        delimiter="\t",
        lineterminator="\n",
        extrasaction="raise",
    )
    writer.writeheader()
    writer.writerows(rows)
    content = stream.getvalue()
    if path.is_file() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    # Parallel build edges (one delink per image) may rewrite the same
    # generated file; a per-process temporary keeps the replace atomic.
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    temporary.write_text(content, encoding="utf-8", newline="")
    temporary.replace(path)


def parse_psx_exe(path: Path) -> ImageLayout:
    data = path.read_bytes()
    if len(data) < 0x800 or data[:8] != b"PS-X EXE":
        raise ValueError(f"{path}: not a PS-X EXE")
    entry, _gp, load_address, load_size = struct.unpack_from("<4I", data, 0x10)
    if len(data) < 0x800 + load_size:
        raise ValueError(f"{path}: truncated load image")
    return ImageLayout(
        path.name.upper(), sha256_path(path), len(data), entry, load_address, load_size
    )


def _require_fields(path: Path, actual: tuple[str, ...], expected: tuple[str, ...]) -> None:
    if actual != expected:
        raise ValueError(f"{path}: fields {actual!r}, expected {expected!r}")


def _require_sorted(path: Path, rows: list[dict[str, str]], key) -> None:
    keys = [key(row) for row in rows]
    if keys != sorted(keys):
        raise ValueError(f"{path}: rows are not canonically sorted")


def _validate_image(row: dict[str, str], path: Path) -> ImageLayout:
    image = row["image"]
    try:
        return IMAGE_LAYOUTS[image]
    except KeyError as error:
        raise ValueError(f"{path}: unknown image {image!r}") from error


def _merged_coverage(intervals: list[tuple[int, int]]) -> list[tuple[int, int]]:
    merged: list[tuple[int, int]] = []
    for start, end in sorted(intervals):
        if start >= end:
            continue
        if merged and start <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], end))
        else:
            merged.append((start, end))
    return merged


def validate_config(config_dir: Path, exe_dir: Path | None = None) -> dict[str, int]:
    function_path = config_dir / "functions.tsv"
    data_path = config_dir / "data.tsv"
    reloc_path = config_dir / "relocs.tsv"
    function_fields, functions = read_tsv(function_path)
    data_fields, data = read_tsv(data_path)
    reloc_fields, relocs = read_tsv(reloc_path)
    _require_fields(function_path, function_fields, FUNCTION_FIELDS)
    _require_fields(data_path, data_fields, DATA_FIELDS)
    _require_fields(reloc_path, reloc_fields, RELOC_FIELDS)

    _require_sorted(
        function_path,
        functions,
        lambda row: (IMAGE_ORDER[row["image"]], parse_int(row["va"])),
    )
    _require_sorted(
        data_path,
        data,
        lambda row: (IMAGE_ORDER[row["image"]], parse_int(row["va"]), row["kind"]),
    )
    _require_sorted(
        reloc_path,
        relocs,
        lambda row: (
            IMAGE_ORDER[row["image"]],
            parse_int(row["site_va"]),
            parse_int(row["paired_site_va"]) if row["paired_site_va"] else -1,
            row["kind"],
            parse_int(row["target_va"]),
        ),
    )

    starts: set[tuple[str, int]] = set()
    functions_by_start: dict[tuple[str, int], dict[str, str]] = {}
    function_intervals: dict[str, list[tuple[int, int]]] = {
        image: [] for image in IMAGE_LAYOUTS
    }
    coverage: dict[str, list[tuple[int, int]]] = {image: [] for image in IMAGE_LAYOUTS}
    for row in functions:
        layout = _validate_image(row, function_path)
        address = parse_int(row["va"])
        size = parse_int(row["size"])
        body_size = parse_int(row["body_size"])
        fragments = int(row["fragments"])
        if address & 3:
            raise ValueError(f"{function_path}: unaligned function {row['image']} {row['va']}")
        if not layout.contains(address, size) or size <= 0:
            raise ValueError(f"{function_path}: invalid extent {row!r}")
        if body_size <= 0 or body_size > size or fragments <= 0:
            raise ValueError(f"{function_path}: invalid body metrics {row!r}")
        if parse_int(row["file_offset"]) != layout.file_offset(address):
            raise ValueError(f"{function_path}: wrong file offset {row!r}")
        key = (row["image"], address)
        if key in starts:
            raise ValueError(f"{function_path}: duplicate start {key!r}")
        starts.add(key)
        functions_by_start[key] = row
        function_intervals[row["image"]].append((address, address + size))
        coverage[row["image"]].append((address, address + size))

    for image, layout in IMAGE_LAYOUTS.items():
        if (image, layout.entry) not in starts:
            raise ValueError(f"{function_path}: missing {image} entry {layout.entry:#x}")
        previous_end = -1
        for start, end in sorted(function_intervals[image]):
            if start < previous_end:
                raise ValueError(
                    f"{function_path}: overlapping {image} function at {start:#x}"
                )
            previous_end = end

    data_keys: set[tuple[str, int, str]] = set()
    data_intervals: dict[str, list[tuple[int, int]]] = {
        image: [] for image in IMAGE_LAYOUTS
    }
    for row in data:
        layout = _validate_image(row, data_path)
        address = parse_int(row["va"])
        size = parse_int(row["size"])
        if not layout.contains(address, size) or size <= 0:
            raise ValueError(f"{data_path}: invalid extent {row!r}")
        if parse_int(row["file_offset"]) != layout.file_offset(address):
            raise ValueError(f"{data_path}: wrong file offset {row!r}")
        key = (row["image"], address, row["kind"])
        if key in data_keys:
            raise ValueError(f"{data_path}: duplicate row {key!r}")
        data_keys.add(key)
        data_intervals[row["image"]].append((address, address + size))
        coverage[row["image"]].append((address, address + size))

    for image, intervals in data_intervals.items():
        previous_end = -1
        for start, end in sorted(intervals):
            if start < previous_end:
                raise ValueError(f"{data_path}: overlapping {image} data at {start:#x}")
            previous_end = end

    for image, layout in IMAGE_LAYOUTS.items():
        merged = _merged_coverage(coverage[image])
        if merged != [(layout.load_address, layout.load_end)]:
            raise ValueError(
                f"{config_dir}: {image} payload is not fully accounted for: {merged!r}"
            )

    reloc_keys: set[tuple[object, ...]] = set()
    for row in relocs:
        layout = _validate_image(row, reloc_path)
        site = parse_int(row["site_va"])
        target = parse_int(row["target_va"])
        paired = parse_int(row["paired_site_va"]) if row["paired_site_va"] else None
        if site & 3 or not layout.contains(site, 4):
            raise ValueError(f"{reloc_path}: invalid site {row!r}")
        if parse_int(row["site_file_offset"]) != layout.file_offset(site):
            raise ValueError(f"{reloc_path}: wrong site file offset {row!r}")
        if paired is not None and (paired & 3 or not layout.contains(paired, 4)):
            raise ValueError(f"{reloc_path}: invalid paired site {row!r}")
        key = (row["image"], site, paired, row["kind"], target)
        if key in reloc_keys:
            raise ValueError(f"{reloc_path}: duplicate candidate {key!r}")
        reloc_keys.add(key)
        if row["status"] not in {"candidate", "reviewed", "rejected"}:
            raise ValueError(f"{reloc_path}: invalid status {row['status']!r}")
        if (
            row["status"] != "rejected"
            and row["kind"] == "mips26"
            and row["opcode"] == "jal"
            and row["channel"] == "reachable-code"
            and row["target_region"] == "load"
            and (row["image"], target) not in starts
        ):
            raise ValueError(
                f"{reloc_path}: reachable call target lacks a function row {row!r}"
            )

    if exe_dir is not None:
        for name, expected in IMAGE_LAYOUTS.items():
            actual = parse_psx_exe(exe_dir / name)
            if actual != expected:
                raise ValueError(f"{exe_dir / name}: retail identity/layout mismatch")

    counts = {
        "functions": len(functions),
        "data": len(data),
        "relocs": len(relocs),
    }
    vendored_path = config_dir / "functions_vendored.tsv"
    if vendored_path.is_file():
        vendored_fields, vendored = read_tsv(vendored_path)
        _require_fields(vendored_path, vendored_fields, VENDORED_FUNCTION_FIELDS)
        _require_sorted(
            vendored_path,
            vendored,
            lambda row: (IMAGE_ORDER[row["image"]], parse_int(row["va"])),
        )
        vendored_keys: set[tuple[str, int]] = set()
        for row in vendored:
            layout = _validate_image(row, vendored_path)
            address = parse_int(row["va"])
            size = parse_int(row["size"])
            key = (row["image"], address)
            if key in vendored_keys:
                raise ValueError(f"{vendored_path}: duplicate provider row {key!r}")
            vendored_keys.add(key)
            try:
                structural = functions_by_start[key]
            except KeyError as error:
                raise ValueError(
                    f"{vendored_path}: provider row lacks structural function {key!r}"
                ) from error
            if size != parse_int(structural["size"]):
                raise ValueError(
                    f"{vendored_path}: size differs from structural function {row!r}"
                )
            if not layout.contains(address, size):
                raise ValueError(f"{vendored_path}: invalid extent {row!r}")
            if parse_int(row["member_offset"]) < 0:
                raise ValueError(f"{vendored_path}: negative member offset {row!r}")
            for field in (
                "provider", "library", "module", "source_version", "evidence",
                "confidence", "provenance", "note",
            ):
                if not row[field]:
                    raise ValueError(f"{vendored_path}: empty {field} in {row!r}")
            if row["confidence"] not in VENDORED_CONFIDENCE:
                raise ValueError(
                    f"{vendored_path}: invalid confidence {row['confidence']!r}"
                )
            names = [row["name"], *row["aliases"].split(";")]
            nonempty_names = [name for name in names if name]
            if len(nonempty_names) != len(set(nonempty_names)):
                raise ValueError(f"{vendored_path}: duplicate names in {row!r}")
        counts["vendored_functions"] = len(vendored)
    if (config_dir / "data_contributions.tsv").is_file():
        from scripts.kf.config_data import load as load_data_contributions

        counts["data_contributions"] = len(load_data_contributions(config_dir))
    return counts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "config_dir",
        nargs="?",
        type=Path,
        default=Path("config/retail"),
    )
    parser.add_argument("--exe-dir", type=Path)
    args = parser.parse_args()
    counts = validate_config(args.config_dir, args.exe_dir)
    print(
        "retail census valid: "
        + ", ".join(f"{name}={count}" for name, count in counts.items())
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
