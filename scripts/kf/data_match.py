"""kf.data_match - data-section matching of reconstruction vs retail.

objdiff scores each unit's ``.text`` against the object carved out of retail;
it also reports a per-section ``.data``/``.rodata`` percentage, but not WHERE a
data section diverges, nor the ``None``-scored case where the reconstruction
emits no section at all. This module supplies that: for every manifested unit
that owns data it compares the reconstruction object's initialized data
sections against the retail-delinked target object, including implicit REL
addends, and reports the first divergence and any referent mismatch. Each BSS
section is compared by allocation class, extent, and named-object layout and
linkage; uninitialized storage has no payload bytes to match. Both objects'
data claims must also admit a single retail base per section satisfying the
actual ELF alignment, using the same placement rules as the relink verifier.

    kf verify data                 # strict gate + per-image summary
    kf verify data --image game    # one image
    kf verify data --coverage      # claimed ranges vs the loaded data census
    kf verify data --detail        # per-diverging-unit byte/referent detail

Any byte, extent, relocation type/referent/addend, or BSS ownership divergence exits
nonzero. Missing comparison objects are also fatal rather than silently
skipped.

The target is the delinked object under ``build/delink/<img>/modules`` whose
``.data``/``.rodata`` bytes are carved under the curated ownership model; the
reconstruction is ``build/objdiff/<img>/base/<obj>``. Both are built by
``kf analyze``; this reads them, so refresh the derived analysis first.

Explicit config-owned SDK contributions are checked separately through
``config_data``: whole provider/target objects, current native data-only reports,
and independent relinking of both sides. They do not enter game progress.
"""

from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass, field, replace
from itertools import zip_longest
from pathlib import Path

from elftools.elf.elffile import ELFFile

from scripts.kf.delink import image_key
from scripts.kf.linked_symbols import linked_addresses
from scripts.kf.manifest import Manifest, Unit, load as load_manifest
from scripts.kf.mips_elf import R_MIPS_32
from scripts.kf.paths import BUILD, RETAIL_CONFIG
from scripts.kf.retail import IMAGE_LAYOUTS, read_tsv


INIT_SECTIONS = (".data", ".rodata", ".sdata")
BSS_SECTIONS = (".bss", ".sbss")


# --------------------------------------------------------------------------- #
# Minimal ELF32 little-endian reader (the delinker's writer is one-directional)
# --------------------------------------------------------------------------- #
@dataclass(frozen=True)
class Reloc:
    offset: int
    type: int
    symbol: str


@dataclass(frozen=True)
class Section:
    name: str
    type: int
    size: int
    data: bytes
    flags: int = 0


@dataclass(frozen=True, order=True)
class Allocation:
    name: str
    offset: int
    size: int
    binding: int
    visibility: int


@dataclass(frozen=True)
class CommonRequest:
    name: str
    size: int
    alignment: int


class Elf:
    """Just enough of an ELF32-LE relocatable object to read data + relocs."""

    def __init__(self, path: Path) -> None:
        blob = path.read_bytes()
        if blob[:4] != b"\x7fELF":
            raise ValueError(f"{path}: not an ELF object")
        e_shoff = struct.unpack_from("<I", blob, 0x20)[0]
        e_shentsize = struct.unpack_from("<H", blob, 0x2E)[0]
        e_shnum = struct.unpack_from("<H", blob, 0x30)[0]
        e_shstrndx = struct.unpack_from("<H", blob, 0x32)[0]
        raw = []
        for i in range(e_shnum):
            base = e_shoff + i * e_shentsize
            (name, typ, flags, _addr, offset, size, link, info,
             _align, entsize) = struct.unpack_from("<10I", blob, base)
            raw.append((name, typ, offset, size, link, info, entsize, flags))
        shstr = raw[e_shstrndx][2]

        def name_at(table_off: int, rel: int) -> str:
            end = blob.index(b"\x00", table_off + rel)
            return blob[table_off + rel:end].decode("ascii", "replace")

        self.sections: dict[str, Section] = {}
        self._sec_by_index: dict[int, str] = {}
        symtab_idx = None
        for idx, (name, typ, offset, size, _link, _info, _ent, flags) in enumerate(raw):
            sname = name_at(shstr, name)
            self._sec_by_index[idx] = sname
            body = b"" if typ == 8 else blob[offset:offset + size]  # SHT_NOBITS=8
            self.sections[sname] = Section(sname, typ, size, body, flags)
            if typ == 2:  # SHT_SYMTAB
                symtab_idx = idx

        self._symbols: list[str] = []
        self.allocations: dict[str, list[Allocation]] = {}
        self.common_requests: list[CommonRequest] = []
        if symtab_idx is not None:
            _, _, sym_off, sym_size, link, _info, entsize, _flags = raw[symtab_idx]
            str_off = raw[link][2]
            entsize = entsize or 16
            for pos in range(sym_off, sym_off + sym_size, entsize):
                st_name, value, size, info, other, st_shndx = struct.unpack_from(
                    "<IIIBBH", blob, pos)
                if st_name:
                    self._symbols.append(name_at(str_off, st_name))
                else:
                    # a section symbol has no name; resolve it to the section it
                    # points at so a jump-table reloc compares as `.text`/`.rodata`
                    # rather than an indistinguishable empty string.
                    self._symbols.append(self._sec_by_index.get(st_shndx, ""))
                section = self._sec_by_index.get(st_shndx)
                if st_name and st_shndx == 0xFFF2:  # SHN_COMMON
                    self.common_requests.append(CommonRequest(
                        self._symbols[-1], size, value))
                if st_name and section and info & 15 not in (3, 4):
                    self.allocations.setdefault(section, []).append(Allocation(
                        self._symbols[-1], value, size, info >> 4, other & 3))

        self._relocs: dict[str, list[Reloc]] = {}
        for idx, (_name, typ, offset, size, _link, info, entsize, _flags) in enumerate(raw):
            if typ != 9:  # SHT_REL
                continue
            target = self._sec_by_index.get(info)
            if target is None:
                continue
            entsize = entsize or 8
            entries: list[Reloc] = []
            for pos in range(offset, offset + size, entsize):
                r_offset, r_info = struct.unpack_from("<II", blob, pos)
                sym_index = r_info >> 8
                r_type = r_info & 0xFF
                symbol = (self._symbols[sym_index]
                          if sym_index < len(self._symbols) else "")
                entries.append(Reloc(r_offset, r_type, symbol))
            self._relocs[target] = entries

    def relocations(self, section: str) -> list[Reloc]:
        return self._relocs.get(section, [])


# --------------------------------------------------------------------------- #
# Per-unit data diff
# --------------------------------------------------------------------------- #
@dataclass
class SectionDiff:
    name: str
    retail_size: int
    recon_size: int
    status: str  # match | missing | extra | size | bytes | referent | addend | storage | layout | placement
    detail: str = ""


@dataclass
class UnitDataDiff:
    image: str
    unit: str
    diffs: list[SectionDiff] = field(default_factory=list)

    @property
    def matches(self) -> bool:
        return all(d.status == "match" for d in self.diffs)

    @property
    def divergent(self) -> list[SectionDiff]:
        return [d for d in self.diffs if d.status != "match"]


@dataclass(frozen=True)
class ArtifactFailure:
    image: str
    unit: str
    detail: str


def _diff_init_section(name: str, retail: Elf, recon: Elf) -> SectionDiff | None:
    rt = retail.sections.get(name)
    rc = recon.sections.get(name)
    rt_size = rt.size if rt else 0
    rc_size = rc.size if rc else 0
    # A section counts as present only when it carries bytes; a compiler-emitted
    # zero-length `.data` is not ownership.
    rt_present = rt is not None and rt_size > 0
    rc_present = rc is not None and rc_size > 0
    if not rt_present and not rc_present:
        return None
    if rt_present and not rc_present:
        return SectionDiff(name, rt_size, rc_size, "missing",
                           f"retail has {rt_size} B ({len(retail.relocations(name))} "
                           f"reloc), reconstruction emits none")
    if rc_present and not rt_present:
        return SectionDiff(name, rt_size, rc_size, "extra",
                           f"reconstruction has {rc_size} B, retail has none")
    if (rt.type, rt.flags) != (rc.type, rc.flags):
        return SectionDiff(name, rt_size, rc_size, "class",
                           "initialized allocation type/flags differ")
    rt_rel = retail.relocations(name)
    rc_rel = recon.relocations(name)
    # Strict means the complete object-section extent is part of the comparison.
    # Probe assembler padding outside the retail claims is an actual extent
    # mismatch, not storage to synthesize in the target or forgive here.
    if rt_size != rc_size:
        return SectionDiff(
            name,
            rt_size,
            rc_size,
            "size",
            f"section extent differs ({rt_size} B retail vs {rc_size} B reconstruction)",
        )
    rt_key = [(r.offset, r.type, r.symbol) for r in rt_rel]
    rc_key = [(r.offset, r.type, r.symbol) for r in rc_rel]
    if rt_key != rc_key:
        # Do not collapse rows into a site-keyed dict: duplicate relocations
        # and their order affect the linker operation too.
        index, (left, right) = next(
            (i, pair) for i, pair in enumerate(zip_longest(rt_key, rc_key))
            if pair[0] != pair[1]
        )
        detail = f"relocation row {index}: retail {left} vs reconstruction {right}"
        return SectionDiff(name, rt_size, rc_size, "referent", detail)
    # Both inputs are relocatable ELF objects, not linked absolute pointers.
    # The delinker has already replaced linked fields with implicit REL
    # addends. Masking them would accept owner+4 for owner+8 and even accept a
    # different case label in a .text-relative switch table.
    first = next((i for i in range(rt_size) if rt.data[i] != rc.data[i]), None)
    if first is not None:
        relocation = next(
            (r for r in rt_rel if r.offset <= first < r.offset + 4), None
        )
        if relocation is not None and relocation.type == R_MIPS_32:  # S + A
            offset = relocation.offset
            left = struct.unpack_from("<I", rt.data, offset)[0]
            right = struct.unpack_from("<I", rc.data, offset)[0]
            return SectionDiff(
                name, rt_size, rc_size, "addend",
                f"reloc +{offset:#x} {relocation.symbol}: "
                f"retail addend {left:#x} vs reconstruction {right:#x}",
            )
        return SectionDiff(name, rt_size, rc_size, "bytes",
                           f"content diverges at +{first:#x} "
                           f"(retail {rt.data[first]:#04x} vs "
                           f"reconstruction {rc.data[first]:#04x})")
    return SectionDiff(name, rt_size, rc_size, "match")


def _diff_bss(retail: Elf, recon: Elf, name: str = ".bss",
              unit: Unit | None = None) -> SectionDiff | None:
    rt, rc = retail.sections.get(name), recon.sections.get(name)
    rt_size, rc_size = rt.size if rt else 0, rc.size if rc else 0
    left = sorted(retail.allocations.get(name, ()))
    right = sorted(recon.allocations.get(name, ()))
    if not (rt_size or rc_size or left or right):
        return None
    if rt is None or rc is None:
        if name == ".bss" and rt is not None and rc is None and unit is not None:
            claimed = {datum.symbol for datum in unit.data
                       if datum.section_name == name and datum.scope == "global"}
            requests = {request.name for request in recon.common_requests}
            if claimed and requests == claimed and len(requests) == len(recon.common_requests):
                return SectionDiff(
                    name, rt_size, rc_size, "unresolved-common-placement",
                    "retail has a synthetic BSS section; reconstruction has only named "
                    "linker COMMON requests. Linked bytes do not prove their original "
                    "input sections or reservation extents",
                )
        return SectionDiff(name, rt_size, rc_size, "missing" if rc is None else "extra",
                           "allocation section is absent on one side")
    if rt.type != 8 or rc.type != 8 or rt.flags != rc.flags:
        return SectionDiff(name, rt_size, rc_size, "storage",
                           f"NOBITS type/flags differ: retail {rt.type}/{rt.flags:#x}, "
                           f"reconstruction {rc.type}/{rc.flags:#x}")
    if rc_size != rt_size:
        return SectionDiff(name, rt_size, rc_size, "size",
                           f"reconstruction owns {rc_size} B, retail {rt_size} B")
    if left != right:
        first = next(pair for pair in zip_longest(left, right) if pair[0] != pair[1])
        return SectionDiff(name, rt_size, rc_size, "layout",
                           f"named allocation differs: retail {first[0]}, "
                           f"reconstruction {first[1]}")
    return SectionDiff(name, rt_size, rc_size, "match")


def _data_placement_issues(path: Path, unit: Unit) -> list[tuple[str, dict]]:
    from scripts.kf.roundtrip import UnitResult, plan

    result = UnitResult(unit.image, unit.unit, str(path))
    # This is a data gate: non-exact function sizes must not masquerade as
    # data-placement failures. All DATA/RODATA claims and ELF sections remain.
    with path.open("rb") as stream:
        plan(ELFFile(stream), replace(unit, functions=()), result)
    owned = {d.symbol: d.section_name for d in unit.data}
    return [(issue.get("section", owned.get(issue.get("symbol"), "object")), issue)
            for issue in result.issues if issue.get("section") != ".text"]


def linked_common_proof(unit: Unit, recon: Elf,
                        linked: dict[str, int] | None) -> frozenset[str]:
    """COMMON requests the current link placed at their exact source claims.

    An object cannot place COMMON; PSYLINK does, by symbol-hash bucket. Only
    when every request of the unit is a claimed global .bss datum that the
    current, source-hash-checked link put at its claimed address (with at
    least its claimed extent) is the retail placement proven. Otherwise
    nothing is proven and the unresolved verdict stands.
    """
    if not linked or not recon.common_requests:
        return frozenset()
    claims = {datum.symbol: datum for datum in unit.data
              if datum.section_name == ".bss" and datum.scope == "global"}
    for request in recon.common_requests:
        claim = claims.get(request.name)
        if (claim is None or linked.get(request.name) != claim.va
                or request.size < claim.size):
            return frozenset()
    return frozenset(request.name for request in recon.common_requests)


def _proven_issue(label: str, issue: dict, proven: frozenset[str]) -> bool:
    """Whether a placement issue only restates COMMON placement the link proved."""
    kind = issue.get("kind")
    if label == "reconstruction" and kind == "unresolved-common-placement":
        return issue.get("symbol") in proven
    if label == "reconstruction" and kind == "owned-symbol-layout":
        return (issue.get("symbol") in proven
                and issue.get("actual_section") == "SHN_COMMON"
                and issue.get("expected_section") == ".bss"
                and issue.get("actual_size", 0) >= issue.get("expected_size", 1))
    if label == "target" and kind == "conflicting-section-bases":
        claims = issue.get("claims", ())
        return (issue.get("section") == ".bss" and bool(claims)
                and all(claim.get("symbol") in proven for claim in claims))
    return False


def diff_unit(unit: Unit, delink_dir: Path, objdiff_dir: Path,
              linked: dict[str, int] | None = None) -> UnitDataDiff | None:
    key, object_name = image_key(unit.image), unit.object_name
    target = delink_dir / key / "modules" / object_name
    base = objdiff_dir / key / "base" / object_name
    missing = [str(path) for path in (target, base) if not path.is_file()]
    if missing:
        raise FileNotFoundError("missing " + ", ".join(missing))
    retail = Elf(target)
    recon = Elf(base)
    # A single named COMMON reservation can be placed in BSS by the linker
    # when its exact source claim supplies the address. The roundtrip plan
    # checks that placement independently; unclaimed COMMON stays divergent.
    from scripts.kf.roundtrip import named_common_bss_claim
    with base.open('rb') as stream:
        common_claim = named_common_bss_claim(ELFFile(stream), unit)
    if common_claim is not None:
        recon.sections['.bss'] = Section('.bss', 8, common_claim.size, b'', 3)
        recon.allocations['.bss'] = [Allocation(
            common_claim.symbol, 0, common_claim.size, 1, 0)]
    result = UnitDataDiff(unit.image, unit.unit)
    for name in INIT_SECTIONS:
        diff = _diff_init_section(name, retail, recon)
        if diff is not None:
            result.diffs.append(diff)
    bss_names = set(BSS_SECTIONS) | {
        section.name for elf in (retail, recon) for section in elf.sections.values()
        if section.type == 8 and section.flags & 2  # allocated NOBITS, including custom names
    }
    proven = linked_common_proof(unit, recon, linked)
    for name in sorted(bss_names):
        bss = _diff_bss(retail, recon, name, unit)
        if bss is not None and bss.status == "unresolved-common-placement" and proven:
            bss = SectionDiff(name, bss.retail_size, bss.recon_size, "match",
                              "COMMON placed at every claim by the current link")
        if bss is not None:
            result.diffs.append(bss)
    by_section = {diff.name: diff for diff in result.diffs}
    for label, path in (("target", target), ("reconstruction", base)):
        for name, issue in _data_placement_issues(path, unit):
            if _proven_issue(label, issue, proven):
                continue
            diff = by_section.get(name)
            if diff is None:
                rt, rc = retail.sections.get(name), recon.sections.get(name)
                diff = SectionDiff(name, rt.size if rt else 0, rc.size if rc else 0, "placement")
                result.diffs.append(diff)
                by_section[name] = diff
            elif diff.status == "match":
                diff.status = "placement"
            detail = f"{label}: {json.dumps(issue, sort_keys=True)}"
            diff.detail = f"{diff.detail}; {detail}" if diff.detail else detail
    return result if result.diffs else None


def diff_image(
    image: str,
    manifest: Manifest,
    delink_dir: Path,
    objdiff_dir: Path,
) -> tuple[list[UnitDataDiff], list[ArtifactFailure]]:
    results: list[UnitDataDiff] = []
    failures: list[ArtifactFailure] = []
    linked = linked_addresses(image)
    for unit in manifest.units:
        if unit.image != image:
            continue
        key = image_key(image)
        target = delink_dir / key / "modules" / unit.object_name
        base = objdiff_dir / key / "base" / unit.object_name
        missing = [str(path) for path in (target, base) if not path.is_file()]
        if missing:
            failures.append(ArtifactFailure(
                image, unit.unit, "missing " + ", ".join(missing)
            ))
            continue
        diff = diff_unit(unit, delink_dir, objdiff_dir, linked)
        if diff is not None:
            results.append(diff)
    return results, failures


# --------------------------------------------------------------------------- #
# Coverage: source claims intersected with the loaded data census
# --------------------------------------------------------------------------- #
def _union(ranges: list[tuple[int, int]]) -> list[tuple[int, int]]:
    """Union of half-open address intervals, without double-counting overlaps."""
    result: list[tuple[int, int]] = []
    for start, end in sorted(ranges):
        if start >= end:
            continue
        if result and start <= result[-1][1]:
            result[-1] = result[-1][0], max(end, result[-1][1])
        else:
            result.append((start, end))
    return result


def _subtract(start: int, end: int, owners: list[tuple[int, int]]) -> list[tuple[int, int]]:
    result = []
    for low, high in owners:
        if high <= start:
            continue
        if low >= end:
            break
        if low > start:
            result.append((start, low))
        start = max(start, high)
    if start < end:
        result.append((start, end))
    return result


def coverage(
    image: str, manifest: Manifest, *, config_dir: Path = RETAIL_CONFIG,
) -> tuple[int, int, list[tuple[int, int, str]]]:
    """Claimed census bytes, total census bytes, and unclaimed interval fragments.

    This is not reachable-data coverage. data.tsv also contains SDK objects,
    unresolved gaps and loaded BSS footprints. Neither those rows nor mere
    references prove complete source ownership. Count retail address ranges,
    not packed object sizes or synthetic assembler alignment tails.
    """
    _fields, data_rows = read_tsv(config_dir / "data.tsv")
    rows = [row for row in data_rows if row["image"] == image]
    census = _union([
        (int(row["va"], 0), int(row["va"], 0) + int(row["size"], 0))
        for row in rows
    ])
    claims = []
    for unit in manifest.units:
        if unit.image != image:
            continue
        claims.extend((d.va, d.va + d.size) for d in unit.data if d.storage == "load")
        for owned in (unit.rodata, unit.sdata):
            if owned:
                start, size = owned
                claims.append((start, start + size))
    owners = _union(claims)
    total = sum(end - start for start, end in census)
    unowned = sum(
        high - low for start, end in census
        for low, high in _subtract(start, end, owners)
    )
    extents = []
    for row in rows:
        start, size = int(row["va"], 0), int(row["size"], 0)
        name = row.get("name") or row.get("kind", "")
        extents.extend(
            (low, high - low, name) for low, high in _subtract(start, start + size, owners)
        )
    extents.sort(key=lambda item: (-item[1], item[0], item[2]))
    return total - unowned, total, extents[:15]


# --------------------------------------------------------------------------- #
def run(images: tuple[str, ...], *, show_detail: bool, show_coverage: bool,
        delink_dir: Path, objdiff_dir: Path) -> int:
    manifest = load_manifest()
    total_units = total_match = total_diverge = total_artifact_failures = 0
    for image in images:
        # diff_image returns every data-owning unit: a matching one carries only
        # `match` SectionDiffs, a diverging one carries at least one other.
        results, artifact_failures = diff_image(
            image, manifest, delink_dir, objdiff_dir
        )
        diverging = [r for r in results if not r.matches]
        matched = len(results) - len(diverging)
        print(f"\n{image}: {len(results)} unit(s) own data; "
              f"{matched} match retail, {len(diverging)} diverge")
        total_units += len(results)
        total_match += matched
        total_diverge += len(diverging)
        total_artifact_failures += len(artifact_failures)
        for result in diverging:
            summary = ", ".join(f"{d.name} [{d.status}]" for d in result.divergent)
            print(f"  DIVERGE {result.unit:<40} {summary}")
            if show_detail:
                for d in result.divergent:
                    print(f"          {d.name:<9} retail={d.retail_size:<6} "
                          f"recon={d.recon_size:<6} {d.detail}")
        for failure in artifact_failures:
            print(f"  INCOMPLETE {failure.unit:<39} {failure.detail}")
        if show_coverage:
            owned, total, extents = coverage(image, manifest)
            pct = (100.0 * owned / total) if total else 0.0
            print(f"  source claims: {owned}/{total} B ({pct:.1f}%) of loaded data census")
            print("  not reachability coverage; census includes SDK data and unresolved gaps")
            print("  largest census fragments without initialized DATA/RODATA claims:")
            for va, size, name in extents[:10]:
                print(f"    {va:#010x}  {size:>6} B  {name}")
    print(f"\ndata-match: {total_match}/{total_units} data-owning unit(s) match retail"
          f" ({total_diverge} diverge; "
          f"{total_artifact_failures} artifact failure(s))")
    from scripts.kf.config_data import run as check_config_data

    config_bad = check_config_data(images, delink_dir=delink_dir, objdiff_dir=objdiff_dir)
    return int(bool(total_diverge or total_artifact_failures or config_bad))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="kf verify data", description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    aliases = {image_key(image): image for image in IMAGE_LAYOUTS}
    parser.add_argument("--image", action="append", choices=tuple(aliases))
    parser.add_argument("--detail", action="store_true",
                        help="print per-section byte/referent detail for diverging units")
    parser.add_argument("--coverage", action="store_true",
                        help="show source claims within the loaded data census, not reachability")
    parser.add_argument("--delink-dir", type=Path, default=BUILD / "delink")
    parser.add_argument("--objdiff-dir", type=Path, default=BUILD / "objdiff")
    args = parser.parse_args(argv)
    images = tuple(aliases[value] for value in args.image) if args.image \
        else tuple(IMAGE_LAYOUTS)
    return run(images, show_detail=args.detail, show_coverage=args.coverage,
               delink_dir=args.delink_dir, objdiff_dir=args.objdiff_dir)


if __name__ == "__main__":
    raise SystemExit(main())
