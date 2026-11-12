"""Three-way GAME `.T` archive comparison: retail, reconstructed C and Rust.

`cd_archive_open` @0x800184d0 and `cd_archive_read` @0x800182d0 run twice:
once as unchanged retail code and once as the relocated reconstruction, with
the same declared services. The Rust `sector_archive` codec then decodes the
same bytes independently.

Declared services, identical for both machine runs:

- `strcat` and `printf` (BIOS stubs) are modelled in Python;
- `CdSearchFile` returns the archive at a fixed per-slot base sector;
- `CdControl`/`CdRead`/`CdReadSync` serve whole sectors of the real archive
  from the last `CdlSetloc` location and never fail;
- `DisableEvent`/`EnableEvent`, `cd_request_wait_idle` (the request queue is
  idle) and `memory_allocate` (a bump allocator) are recorded.

Services are recorded semantically (strings, locations, sizes, destination
buffers) rather than as raw stack pointers, so a residual frame-size
difference between retail and C cannot mask or fake a behavioural one.
Every non-empty checksummed entry of every archive is read. Entries without a
checksum (`RTIM.T`, the VAB bodies) are never read through this path by the
game; their checksum failure is asserted instead. One entry per archive is
also read with its first transfer corrupted to exercise the reread loop.
"""

from __future__ import annotations

import argparse
import struct
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Sequence

from scripts.kf.codec_candidate import rebuild_units
from scripts.kf.local_config import configured_retail_dir
from scripts.kf.parser_machine import (
    CandidateFunction,
    CandidateProgram,
    ExternalHook,
    GameSymbols,
    HookContext,
    MemoryInput,
    MemoryRange,
    ParserMachine,
    RetailProgram,
)
from scripts.kf.paths import BUILD
from scripts.kf.rust_codec import RustCodec, build_driver
from scripts.kf.sema.image import RetailImage

SECTOR = 0x800
SLOT_SIZE = 12
# KfCdMemory.archives: the slots follow the 0x2a4-byte CD state in cdrom_ctl.
ARCHIVES_OFFSET = 0x2A4
# Slots assigned by func_80015d58, which opens every archive at startup.
SLOTS = {
    "MO.T": 0,
    "RTMD.T": 1,
    "RTIM.T": 2,
    "TALK.T": 3,
    "VAB.T": 4,
    "FDAT.T": 5,
    "ITEM.T": 6,
}
# Base sectors chosen so entry locations carry across seconds and minutes.
BASE_SECTORS = {name: 4_425 + slot * 20_011 for name, slot in SLOTS.items()}

# Oracle-owned RAM: inside .bss that no admitted code touches, below the
# cdrom_ctl archive slots at 0x801b6004 and the stack at 0x801ff000.
NAME_VA = 0x80070000
HEAP_VA = 0x80071000
DEST_VA = 0x80080000
DEST_LIMIT = 0x80180000
HEAP_LIMIT = DEST_VA

GAME_UNITS = ("game.cd_archive", "game.cd_location", "game.cd_checksum", "game.resource_copy")
OBJECTS = {
    "cd_archive_open": "800180b4_cd_archive.o",
    "cd_archive_read": "800180b4_cd_archive.o",
    "cd_extent_read_into": "800180b4_cd_archive.o",
    "cd_read_sectors": "800180b4_cd_archive.o",
    "cd_report_error": "800180b4_cd_archive.o",
    "cd_location_add": "80017aa0_cd_location.o",
    "cd_location_to_sector": "80017aa0_cd_location.o",
    "cd_sector_to_location": "80017aa0_cd_location.o",
    "cd_bcd_to_int": "80017aa0_cd_location.o",
    "cd_int_to_bcd": "80017aa0_cd_location.o",
    "cd_sectors_corrupt": "80017d00_cd_checksum.o",
    "resource_copy_halfwords": "800171c8_resource_copy.o",
}
OPEN_CLOSURE = (
    "cd_archive_open",
    "cd_extent_read_into",
    "cd_read_sectors",
    "cd_report_error",
    "resource_copy_halfwords",
)
READ_CLOSURE = (
    "cd_archive_read",
    "cd_location_add",
    "cd_location_to_sector",
    "cd_sector_to_location",
    "cd_bcd_to_int",
    "cd_int_to_bcd",
    "cd_read_sectors",
    "cd_report_error",
    "cd_sectors_corrupt",
)


class OracleMismatch(AssertionError):
    pass


def require(label: str, condition: bool, detail: str) -> None:
    if not condition:
        raise OracleMismatch(f"{label}: {detail}")


def bcd(value: int) -> int:
    value &= 0xFF
    return ((value // 10) << 4 | value % 10) & 0xFF


def location_bytes(sector: int) -> bytes:
    rest = sector % 4_500
    return bytes((bcd(sector // 4_500), bcd(rest // 75), bcd(rest % 75), 0))


def location_sector(location: bytes) -> int:
    def value(byte: int) -> int:
        return (byte >> 4) * 10 + (byte & 0x0F)

    return value(location[0]) * 4_500 + value(location[1]) * 75 + value(location[2])


def c_string(context: HookContext, address: int, limit: int = 256) -> bytes:
    data = context.read(address, limit)
    end = data.find(b"\0")
    if end < 0:
        raise OracleMismatch(f"unterminated string at {address:#010x}")
    return data[:end]


@dataclass
class Services:
    """Deterministic CD, heap and BIOS services shared by both machine runs."""

    archive: bytes
    base_sector: int
    corrupt_first_read: bool = False
    events: list[tuple] = field(default_factory=list)
    heap: int = HEAP_VA
    location: int | None = None
    reads: int = 0

    def destination(self, address: int) -> str:
        if DEST_VA <= address < DEST_LIMIT:
            return f"dest+{address - DEST_VA:#x}"
        if HEAP_VA <= address < HEAP_LIMIT:
            return f"heap+{address - HEAP_VA:#x}"
        return "stack"

    def strcat(self, context: HookContext) -> int:
        destination, source = context.args[0], context.args[1]
        joined = c_string(context, destination) + c_string(context, source)
        context.write(destination, joined + b"\0")
        self.events.append(("strcat", joined))
        return destination

    def printf(self, context: HookContext) -> int:
        self.events.append(("printf", c_string(context, context.args[0])))
        return 0

    def search(self, context: HookContext) -> int:
        file_va, path_va = context.args[0], context.args[1]
        self.events.append(("CdSearchFile", c_string(context, path_va)))
        record = location_bytes(self.base_sector) + struct.pack("<I", len(self.archive))
        context.write(file_va, record + bytes(16))
        return file_va

    def wait_idle(self, _context: HookContext) -> int:
        self.events.append(("cd_request_wait_idle",))
        return 0

    def event(self, name: str):
        def handler(context: HookContext) -> int:
            self.events.append((name, context.args[0]))
            return 1

        return handler

    def control(self, context: HookContext) -> int:
        command, parameter = context.args[0] & 0xFF, context.args[1]
        if command == 2:  # CdlSetloc
            location = context.read(parameter, 4)
            self.location = location_sector(location)
            self.events.append(("CdControl", command, location))
        else:
            self.events.append(("CdControl", command, parameter))
        return 1

    def read(self, context: HookContext) -> int:
        sectors, buffer, mode = context.args[0], context.args[1], context.args[2]
        self.events.append(("CdRead", sectors, self.destination(buffer), mode))
        if self.location is None:
            raise OracleMismatch("CdRead before CdlSetloc")
        start = (self.location - self.base_sector) * SECTOR
        data = bytearray(self.archive[start:start + sectors * SECTOR])
        if len(data) != sectors * SECTOR:
            raise OracleMismatch(f"CdRead past the archive at sector {self.location}")
        if self.corrupt_first_read and self.reads == 0:
            data[len(data) // 2] ^= 0xA5
        self.reads += 1
        context.write(buffer, bytes(data))
        return 1

    def read_sync(self, _context: HookContext) -> int:
        return 0

    def allocate(self, context: HookContext) -> int:
        size = context.args[0]
        address = self.heap
        self.heap = (self.heap + size + 3) & ~3
        if self.heap > HEAP_LIMIT:
            raise OracleMismatch(f"oracle heap exhausted by a {size}-byte allocation")
        self.events.append(("memory_allocate", size, address - HEAP_VA))
        return address

    def hooks(self) -> list[ExternalHook]:
        return [
            ExternalHook("strcat", self.strcat),
            ExternalHook("printf", self.printf),
            ExternalHook("CdSearchFile", self.search),
            ExternalHook("cd_request_wait_idle", self.wait_idle),
            ExternalHook("DisableEvent", self.event("DisableEvent")),
            ExternalHook("EnableEvent", self.event("EnableEvent")),
            ExternalHook("CdControl", self.control),
            ExternalHook("CdRead", self.read),
            ExternalHook("CdReadSync", self.read_sync),
            ExternalHook("memory_allocate", self.allocate),
        ]


def program(symbols: GameSymbols, closure: Sequence[str], services: Services, candidate: bool):
    if candidate:
        base = BUILD / "objdiff/game/base"
        return CandidateProgram.link(
            symbols,
            [CandidateFunction(name, base / OBJECTS[name]) for name in closure],
            hooks=services.hooks(),
            bind_data_objects=True,
        )
    return RetailProgram.link(symbols, list(closure), hooks=services.hooks())


@dataclass(frozen=True)
class RunResult:
    memory: dict[str, bytes]
    events: tuple[tuple, ...]
    reads: int
    instructions: int


def run(
    retail: RetailImage,
    symbols: GameSymbols,
    closure: Sequence[str],
    entry: str,
    args: Sequence[int],
    *,
    archive: bytes,
    base_sector: int,
    memory: Sequence[MemoryInput],
    capture: Sequence[MemoryRange],
    allowed: Sequence[MemoryRange],
    candidate: bool,
    corrupt_first_read: bool = False,
) -> RunResult:
    services = Services(archive, base_sector, corrupt_first_read)
    result = ParserMachine(retail, program(symbols, closure, services, candidate)).call(
        entry, list(args), memory=memory, capture=capture, allowed_writes=allowed
    )
    return RunResult(result.memory_by_name(), tuple(services.events), services.reads, result.instructions)


def compare_machines(label: str, retail: RunResult, candidate: RunResult) -> None:
    require(label, retail.events == candidate.events,
            f"service effects differ\n retail    {retail.events}\n candidate {candidate.events}")
    for name, data in retail.memory.items():
        require(label, data == candidate.memory[name], f"{name} differs between retail and C")


def check_open(retail: RetailImage, symbols: GameSymbols, rust: RustCodec,
               name: str, archive: bytes) -> int:
    slot = SLOTS[name]
    slot_va = symbols.datum("cdrom_ctl")[0] + ARCHIVES_OFFSET + slot * SLOT_SIZE
    table, size_block = rust.call("archive-open", archive[:SECTOR])
    table_size = struct.unpack("<I", size_block)[0]
    path = b"COM\\" + name.encode()
    captures = (
        MemoryRange("slot", slot_va, SLOT_SIZE),
        MemoryRange("table", HEAP_VA, table_size),
    )
    allowed = (MemoryRange("slot", slot_va, SLOT_SIZE), MemoryRange("heap", HEAP_VA, HEAP_LIMIT - HEAP_VA))
    runs = [
        run(retail, symbols, OPEN_CLOSURE, "cd_archive_open", [slot, NAME_VA],
            archive=archive, base_sector=BASE_SECTORS[name],
            memory=(MemoryInput(NAME_VA, path + b"\0"),), capture=captures,
            allowed=allowed, candidate=candidate)
        for candidate in (False, True)
    ]
    label = f"open {name}"
    compare_machines(label, *runs)
    memory = runs[0].memory
    expected_slot = struct.pack("<I", HEAP_VA) + location_bytes(BASE_SECTORS[name]) + struct.pack("<I", len(archive))
    require(label, memory["slot"] == expected_slot, "archive slot differs from the Rust extent")
    require(label, memory["table"] == table, "copied offset table differs from Rust")
    require(label, ("memory_allocate", table_size, 0) in runs[0].events, "allocation size differs from Rust")
    require(label, ("CdSearchFile", b"\\CD\\" + path + b";1") in runs[0].events, "searched path differs")
    return runs[0].instructions + runs[1].instructions


def check_read(retail: RetailImage, symbols: GameSymbols, rust: RustCodec,
               name: str, archive: bytes, index: int, *, corrupt: bool) -> int:
    slot = SLOTS[name]
    base = BASE_SECTORS[name]
    entry_bytes, location, count_block, holds = rust.call(
        "archive-read", archive, struct.pack("<I", index), location_bytes(base)
    )
    count = struct.unpack("<I", count_block)[0]
    require(f"read {name}[{index}]", holds == b"\x01", "entry checksum fails in Rust")
    slot_va = symbols.datum("cdrom_ctl")[0] + ARCHIVES_OFFSET + slot * SLOT_SIZE
    table_size = (struct.unpack_from("<H", archive)[0] + 1) * 2
    slot_bytes = struct.pack("<I", HEAP_VA) + location_bytes(base) + struct.pack("<I", len(archive))
    memory = (
        MemoryInput(slot_va, slot_bytes),
        MemoryInput(HEAP_VA, archive[2:2 + table_size]),
    )
    capture = (MemoryRange("destination", DEST_VA, len(entry_bytes)),)
    allowed = (MemoryRange("destination", DEST_VA, len(entry_bytes)),)
    runs = [
        run(retail, symbols, READ_CLOSURE, "cd_archive_read", [slot, index, DEST_VA],
            archive=archive, base_sector=base, memory=memory, capture=capture,
            allowed=allowed, candidate=candidate, corrupt_first_read=corrupt)
        for candidate in (False, True)
    ]
    label = f"read {name}[{index}]{' corrupted-first' if corrupt else ''}"
    compare_machines(label, *runs)
    require(label, runs[0].memory["destination"] == entry_bytes, "entry bytes differ from Rust")
    reads = [event for event in runs[0].events if event[0] == "CdRead"]
    seeks = [event[2] for event in runs[0].events if event[:2] == ("CdControl", 2)]
    expected_reads = 2 if corrupt else 1
    require(label, len(reads) == expected_reads, f"{len(reads)} reads, expected {expected_reads}")
    require(label, all(read[1] == count for read in reads), "sector count differs from Rust")
    require(label, all(seek == location for seek in seeks), "seek location differs from Rust")
    return runs[0].instructions + runs[1].instructions


def archives(disc_dir: Path) -> dict[str, bytes]:
    com = disc_dir / "CD/COM"
    if not com.is_dir():
        raise SystemExit(
            f"{com}: missing; run `kf init --retail-dir` with the extracted SLPS-00069 "
            "disc directory that holds both the executables and CD/COM"
        )
    return {name: (com / name).read_bytes() for name in SLOTS}


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rust-driver", type=Path)
    parser.add_argument("--no-rebuild", action="store_true")
    parser.add_argument("--retail-dir", type=Path, help="extracted disc directory (default: configured)")
    args = parser.parse_args(argv)
    if not args.no_rebuild:
        rebuild_units(GAME_UNITS)
    retail = RetailImage.load("GAME.EXE")
    symbols = GameSymbols.load()
    rust = RustCodec(args.rust_driver or build_driver())
    started = time.monotonic()
    opened = read = corrupted = unchecked = instructions = 0
    for name, archive in archives(configured_retail_dir(args.retail_dir)).items():
        instructions += check_open(retail, symbols, rust, name, archive)
        opened += 1
        count = struct.unpack_from("<H", archive)[0]
        offsets = struct.unpack_from(f"<{count + 1}H", archive, 2)
        corrupt_done = False
        for index in range(count):
            data = archive[offsets[index] * SECTOR:offsets[index + 1] * SECTOR]
            if not data:
                continue
            words = struct.unpack(f"<{len(data) // 4}I", data)
            if (0x12345678 + sum(words[:-1])) & 0xFFFFFFFF != words[-1]:
                (_entry, _location, _count, holds) = rust.call(
                    "archive-read", archive, struct.pack("<I", index), location_bytes(BASE_SECTORS[name])
                )
                require(f"read {name}[{index}]", holds == b"\x00", "Rust accepts an unchecksummed entry")
                unchecked += 1
                continue
            instructions += check_read(retail, symbols, rust, name, archive, index, corrupt=False)
            read += 1
            if not corrupt_done:
                instructions += check_read(retail, symbols, rust, name, archive, index, corrupt=True)
                corrupt_done = True
                corrupted += 1
        print(f"[sector-archive-oracle] PASS {name}: open, {count} entries", flush=True)
    print(
        f"[sector-archive-oracle] PASS: {opened} opens, {read} checksummed reads, "
        f"{corrupted} reread controls, {unchecked} unchecksummed entries rejected "
        f"({instructions} instructions, {time.monotonic() - started:.1f}s)",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
