"""Retarget the curated JP (SLPS-00069) model to the US disc (SLUS-00158).

Reads config/us_port/{function_map,address_map}.tsv and the US executables,
rewrites every address in config/retail, config/evidence, the baseline and the
source claims, and rescans relocations inside functions the US build changed.
Prints everything it could not translate.

    python3 -m scripts.us_port.retarget --us-dir disc/us/exe
"""

from __future__ import annotations

import argparse
import bisect
import csv
import re
import struct
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
US_FILES = {"PSX.EXE": "SLUS_001.58", "GAME.EXE": "GAME.EXE", "OPEN.EXE": "OPEN.EXE", "END.EXE": "END.EXE"}
SHORT = {"PSX": "PSX.EXE", "GAME": "GAME.EXE", "OPEN": "OPEN.EXE", "END": "END.EXE"}
SRC_IMAGE = {"psx": "PSX.EXE", "game": "GAME.EXE", "open": "OPEN.EXE", "end": "END.EXE"}
HEX = re.compile(r"0x80[0-9a-fA-F]{6}\b")
REG = "zero at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 s0 s1 s2 s3 s4 s5 s6 s7 t8 t9 k0 k1 gp sp fp ra".split()
LO_OPS = {9: "addiu", 32: "lb", 33: "lh", 35: "lw", 36: "lbu", 37: "lhu", 40: "sb", 41: "sh", 43: "sw",
          34: "lwl", 38: "lwr", 42: "swl", 46: "swr", 50: "lwc2", 58: "swc2", 13: "ori"}
LOADS = {32, 33, 34, 35, 36, 37, 38}

log: Counter = Counter()
unresolved: list[str] = []


def read_tsv(path: Path):
    comments, header, rows = [], None, []
    for line in path.read_text().splitlines():
        if line.startswith("#"):
            comments.append(line)
            continue
        fields = line.split("\t")
        if header is None:
            header = fields
            continue
        rows.append(dict(zip(header, fields + [""] * (len(header) - len(fields)))))
    return comments, header, rows


ORDER = {"PSX.EXE": 0, "GAME.EXE": 1, "OPEN.EXE": 2, "END.EXE": 3}


def canonical(rows, header):
    """Keep the validator's canonical (image, va[, kind]) order."""
    if "image" not in header or "va" not in header:
        return rows
    return sorted(rows, key=lambda r: (ORDER.get(r["image"], 9), int(r["va"], 16), r.get("kind", "")))


def write_tsv(path: Path, comments, header, rows) -> None:
    rows = canonical(rows, header)
    lines = list(comments) + ["\t".join(header)]
    lines += ["\t".join(row.get(h, "") for h in header) for row in rows]
    path.write_text("\n".join(lines) + "\n")


class UsImage:
    def __init__(self, path: Path):
        data = path.read_bytes()
        self.pc, _, self.t_addr, self.t_size = struct.unpack_from("<4I", data, 0x10)
        self.body = data[0x800:0x800 + self.t_size]
        self.file_size = len(data)

    def word(self, va: int) -> int:
        return struct.unpack_from("<I", self.body, va - self.t_addr)[0]

    @property
    def load_end(self) -> int:
        return self.t_addr + self.t_size


class Translator:
    """JP address -> US address through the function and data maps."""

    def __init__(self):
        self.entries = defaultdict(list)  # image -> [(jp, jp_end, us, us_size, status, kind)]
        self.functions = {}
        for row in csv.DictReader((ROOT / "config/us_port/function_map.tsv").open(), delimiter="\t"):
            if not row["jp_va"]:
                continue
            key = (row["image"], int(row["jp_va"], 16))
            us = int(row["us_va"], 16) if row["us_va"] else None
            self.functions[key] = (us, int(row["us_size"], 16), row["status"])
            self.entries[row["image"]].append(
                (key[1], key[1] + int(row["jp_size"], 16), us, int(row["us_size"], 16), row["status"], "func"))
        for row in csv.DictReader((ROOT / "config/us_port/address_map.tsv").open(), delimiter="\t"):
            if row["kind"] != "data":
                continue
            jp = int(row["jp_va"], 16)
            us = int(row["us_va"], 16) if row["us_va"] else None
            self.entries[row["image"]].append(
                (jp, jp + int(row["jp_size"], 16), us, int(row["us_size"], 16), row["status"], "data"))
        self.data_overrides = {}
        for row in read_tsv(ROOT / "config/us_port/data_overrides.tsv")[2]:
            key = (row["image"], int(row["jp_va"], 16))
            us = int(row["us_va"], 16) if row["us_va"] else None
            self.data_overrides[key] = (us, int(row["us_size"] or "0", 16))
            self.entries[key[0]] = [e for e in self.entries[key[0]] if not (e[5] == "data" and e[0] == key[1])]
            self.entries[key[0]].append((key[1], key[1] + int(row["jp_size"], 16), us, int(row["us_size"] or "0", 16),
                                         "override" if us is not None else "removed", "data"))
        for image in self.entries:
            self.entries[image].sort(key=lambda e: (e[0], e[1]))
        self.starts = {k: [e[0] for e in v] for k, v in self.entries.items()}

    def _delta(self, entry):
        jp, _, us, _, status, kind = entry
        if us is None or status in ("removed",):
            return None
        if kind == "func" and status in ("changed", "us-only"):
            return None
        return us - jp

    def __call__(self, image: str, va: int, end: bool = False) -> int | None:
        if image not in self.entries:
            return None
        entries, starts = self.entries[image], self.starts[image]
        i = bisect.bisect_right(starts, va) - 1
        # innermost containing entry: the latest start that still covers va
        for j in range(i, max(-1, i - 64), -1):
            jp, jp_end, us, _, status, kind = entries[j]
            if jp <= va < jp_end:
                if kind == "func" and status in ("changed", "us-only"):
                    return us if va == jp else None
                d = self._delta(entries[j])
                return None if d is None else va + d
        prev = next((e for e in reversed(entries[:i + 1]) if e[1] <= va and self._delta(e) is not None), None)
        nxt = next((e for e in entries[i + 1:] if e[0] > va and self._delta(e) is not None), None)
        if prev and va == prev[1]:
            return va + self._delta(prev)
        if prev and nxt and self._delta(prev) == self._delta(nxt):
            return va + self._delta(prev)
        return None

    def text(self, image: str, text: str) -> str:
        def sub(m):
            us = self(image, int(m.group(0), 16))
            return m.group(0) if us is None else f"0x{us:08x}"
        return HEX.sub(sub, text)


def translate_col(t: Translator, row: dict, col: str, image: str, what: str, end: bool = False) -> bool:
    value = row.get(col, "")
    if not value:
        return True
    us = t(image, int(value, 16))
    if us is None and end:
        prior = t(image, int(value, 16) - 1)
        us = None if prior is None else prior + 1
    if us is None:
        unresolved.append(f"{what}: {image} {col}={value} {row.get('name') or row.get('target_name') or ''}")
        return False
    row[col] = f"0x{us:08x}"
    return True


def free_text(t: Translator, row: dict, image: str, cols) -> None:
    for col in cols:
        if row.get(col):
            row[col] = t.text(image, row[col])


def file_offset(us: UsImage, va: int) -> str:
    return f"0x{0x800 + va - us.t_addr:x}"


def retarget_functions(t: Translator, images: dict) -> None:
    path = ROOT / "config/retail/functions.tsv"
    comments, header, rows = read_tsv(path)
    out = []
    for row in rows:
        image, jp = row["image"], int(row["va"], 16)
        us, size, status = t.functions.get((image, jp), (None, 0, "removed"))
        if status == "removed" or us is None:
            log["functions.tsv removed"] += 1
            continue
        row["va"] = f"0x{us:08x}"
        row["file_offset"] = file_offset(images[image], us)
        if status == "changed":
            row["size"] = row["body_size"] = f"0x{size:x}"
            row["provenance"] = (row["provenance"] + ";" if row["provenance"] else "") + "us-port:jr-ra-boundary"
        free_text(t, row, image, ("labels", "note"))
        out.append(row)
    for line in (ROOT / "config/us_port/function_map.tsv").read_text().splitlines()[1:]:
        f = line.split("\t")
        if f[5] == "us-only":
            us = int(f[3], 16)
            out.append(dict(image=f[0], va=f"0x{us:08x}", file_offset=file_offset(images[f[0]], us), size=f[4],
                            body_size=f[4], fragments="1", kind="function", confidence="us-port-boundary",
                            name="", labels="", provenance="us-port:jr-ra-boundary",
                            note="US-only function split out of menu_run_root_controller's JP extent"))
    out.sort(key=lambda r: (["PSX.EXE", "GAME.EXE", "OPEN.EXE", "END.EXE"].index(r["image"]), int(r["va"], 16)))
    write_tsv(path, comments, header, out)


def retarget_simple(t: Translator, rel: str, cols, text_cols=(), end_cols=(), drop_unresolved=False) -> None:
    path = ROOT / rel
    comments, header, rows = read_tsv(path)
    out = []
    for row in rows:
        image = row.get("image", "")
        if "va" in cols:
            override = t.data_overrides.get((image, int(row["va"], 16)))
            if override and override[0] is None:
                log[f"{rel} removed (override)"] += 1
                continue
            if override and "size" in row:
                row["size"] = f"0x{override[1]:x}"
        ok = all(translate_col(t, row, c, image, rel) for c in cols)
        ok = all(translate_col(t, row, c, image, rel, end=True) for c in end_cols) and ok
        if not ok and drop_unresolved:
            log[f"{rel} dropped"] += 1
            continue
        free_text(t, row, image, text_cols)
        out.append(row)
    write_tsv(path, comments, header, out)


def retarget_function_rows(t: Translator, rel: str, text_cols=()) -> None:
    path = ROOT / rel
    comments, header, rows = read_tsv(path)
    out = []
    for row in rows:
        image, jp = row["image"], int(row["va"], 16)
        us, size, status = t.functions.get((image, jp), (None, 0, "removed"))
        if us is None:
            log[f"{rel} removed"] += 1
            continue
        row["va"] = f"0x{us:08x}"
        if "size" in row and status == "changed":
            row["size"] = f"0x{size:x}"
        free_text(t, row, image, text_cols)
        out.append(row)
    write_tsv(path, comments, header, out)


def retarget_data(t: Translator, images: dict) -> None:
    path = ROOT / "config/retail/data.tsv"
    comments, header, rows = read_tsv(path)
    out, seen = [], set()
    for row in rows:
        image = row["image"]
        override = t.data_overrides.get((image, int(row["va"], 16)))
        if override and override[0] is None:
            log["data.tsv removed (override)"] += 1
            continue
        if override:
            row["size"] = f"0x{override[1]:x}"
        if not translate_col(t, row, "va", image, "data.tsv"):
            log["data.tsv dropped"] += 1
            continue
        us = images[image]
        va = int(row["va"], 16)
        if va < us.load_end:
            row["file_offset"] = file_offset(us, va)
            if row["confidence"] == "coverage-gap" and va + int(row["size"], 16) > us.load_end:
                row["size"] = f"0x{us.load_end - va:x}"  # gaps end with the US image
                log["data.tsv coverage gap clamped"] += 1
        free_text(t, row, image, ("note",))
        key = (image, row["va"], row["kind"])
        if key in seen:
            log["data.tsv duplicate after translation dropped"] += 1
            continue
        seen.add(key)
        out.append(row)
    write_tsv(path, comments, header, out)


GAP = dict(kind="unclassified", confidence="coverage-gap", name="", datatype="", provenance="payload-complement",
           note="may contain missed code, tables, constants, padding, or writable data")


def finalize_data(images: dict) -> None:
    """Make data.tsv valid for the US images: no data overlaps, and functions plus
    data cover each load image exactly. Named rows win; census rows yield; gaps refill."""
    path = ROOT / "config/retail/data.tsv"
    comments, header, rows = read_tsv(path)
    functions = read_tsv(ROOT / "config/retail/functions.tsv")[2]
    out = []
    for image, us in images.items():
        mine = [r for r in rows if r["image"] == image and r["confidence"] != "coverage-gap"]
        kept: list[tuple[int, int]] = []
        for named in (True, False):
            for row in sorted((r for r in mine if bool(r["name"]) == named), key=lambda r: int(r["va"], 16)):
                start, end = int(row["va"], 16), int(row["va"], 16) + int(row["size"], 16)
                if start < us.t_addr or end > us.load_end or any(s < end and start < e for s, e in kept):
                    log["data.tsv census row dropped (overlap or outside image)" if not named
                        else "data.tsv NAMED row dropped (overlap or outside image)"] += 1
                    if named:
                        unresolved.append(f"data.tsv: {image} {row['va']} {row['name']} overlaps")
                    continue
                kept.append((start, end))
                out.append(row)
        covered = sorted(kept + [(int(f["va"], 16), int(f["va"], 16) + int(f["size"], 16))
                                 for f in functions if f["image"] == image])
        cursor = us.t_addr
        for start, end in covered + [(us.load_end, us.load_end)]:
            if start > cursor:
                out.append(dict(GAP, image=image, va=f"0x{cursor:08x}", file_offset=file_offset(us, cursor),
                                size=f"0x{start - cursor:x}"))
                log["data.tsv coverage gap"] += 1
            cursor = max(cursor, end)
    write_tsv(path, comments, header, out)


def scan_relocations(image: str, us: UsImage, start: int, size: int, names: dict) -> list[dict]:
    rows, pending = [], {}
    for va in range(start, start + size, 4):
        w = us.word(va)
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        if op in (2, 3):
            target = (va & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
            rows.append(dict(image=image, site_va=f"0x{va:08x}", site_file_offset=f"0x{0x800 + va - us.t_addr:x}",
                             paired_site_va="", kind="mips26", channel="instruction-word",
                             target_va=f"0x{target:08x}", target_region="load", target_name=names.get(target, ""),
                             opcode="jal" if op == 3 else "j", register="", confidence="control-flow",
                             status="candidate", provenance="us-port:scan"))
            continue
        if op == 15:
            pending[rt] = (va, w & 0xFFFF)
            continue
        if op in LO_OPS and rs in pending and op != 13:
            hi_va, hi = pending[rs]
            lo = w & 0xFFFF
            target = ((hi << 16) + (lo - 0x10000 if lo & 0x8000 else lo)) & 0xFFFFFFFF
            if 0x80010000 <= target < 0x80200000:
                rows.append(dict(image=image, site_va=f"0x{hi_va:08x}",
                                 site_file_offset=f"0x{0x800 + hi_va - us.t_addr:x}", paired_site_va=f"0x{va:08x}",
                                 kind="mips_hi16_lo16", channel="instruction-word", target_va=f"0x{target:08x}",
                                 target_region="load" if target < us.load_end else "bss",
                                 target_name=names.get(target, ""), opcode=f"lui+{LO_OPS[op]}", register=REG[rt],
                                 confidence="paired-pattern", status="candidate", provenance="us-port:scan"))
            del pending[rs]
            continue
        # any other write to a pending register ends its pairing window
        dest = rt if op in LOADS or op in (8, 9, 10, 11, 12, 13, 14) else ((w >> 11) & 31 if op == 0 else None)
        if dest in pending:
            del pending[dest]
    # keep the first lo per hi, as the curated table does
    seen, unique = set(), []
    for row in rows:
        if row["site_va"] in seen:
            continue
        seen.add(row["site_va"])
        unique.append(row)
    return unique


def retarget_relocs(t: Translator, images: dict) -> None:
    path = ROOT / "config/retail/relocs.tsv"
    comments, header, rows = read_tsv(path)
    out = []
    for row in rows:
        image = row["image"]
        us_site = t(image, int(row["site_va"], 16))
        if us_site is None:
            log["relocs.tsv site in changed/removed code"] += 1
            continue
        row["site_va"] = f"0x{us_site:08x}"
        row["site_file_offset"] = file_offset(images[image], us_site)
        if row["paired_site_va"]:
            paired = t(image, int(row["paired_site_va"], 16))
            if paired is None:
                log["relocs.tsv paired site unresolved"] += 1
                continue
            row["paired_site_va"] = f"0x{paired:08x}"
        target = int(row["target_va"], 16)
        us_target = t(image, target)
        if us_target is None and row["kind"] == "mips32_candidate":
            # raw words into changed code (jump tables): read the US word itself
            word = images[image].word(us_site)
            if 0x80010000 <= word < 0x80200000:
                us_target = word
                log["relocs.tsv raw word re-read from US"] += 1
        if us_target is None:
            unresolved.append(f"relocs.tsv: {image} target {row['target_va']} from site {row['site_va']}")
            log["relocs.tsv target unresolved"] += 1
            continue
        row["target_va"] = f"0x{us_target:08x}"
        out.append(row)
    names = defaultdict(dict)
    for rel in ("config/retail/function_identities.tsv", "config/retail/data_identities.tsv"):
        for row in read_tsv(ROOT / rel)[2]:
            names[row["image"]][int(row["va"], 16)] = row["name"]
    present = {(r["image"], r["site_va"]) for r in out}
    for line in (ROOT / "config/us_port/function_map.tsv").read_text().splitlines()[1:]:
        f = line.split("\t")
        if f[5] in ("changed", "us-only"):
            for row in scan_relocations(f[0], images[f[0]], int(f[3], 16), int(f[4], 16), names[f[0]]):
                if (row["image"], row["site_va"]) not in present:
                    out.append(row)
                    log["relocs.tsv scanned in changed code"] += 1
    order = ["PSX.EXE", "GAME.EXE", "OPEN.EXE", "END.EXE"]
    out.sort(key=lambda r: (order.index(r["image"]), int(r["site_va"], 16)))
    write_tsv(path, comments, header, out)


def add_us_only_identities() -> None:
    path = ROOT / "config/retail/function_identities.tsv"
    comments, header, rows = read_tsv(path)
    for line in (ROOT / "config/us_port/function_map.tsv").read_text().splitlines()[1:]:
        f = line.split("\t")
        if f[5] == "us-only":
            rows.append(dict(image=f[0], va=f"0x{int(f[3], 16):08x}", name=f[7], return_type="void",
                             name_confidence="address", signature_confidence="unknown",
                             evidence="us-port:jr-ra-boundary", note="US-only; not in the JP build"))
    write_tsv(path, comments, header, rows)


def retarget_baseline(t: Translator) -> None:
    path = ROOT / "config/match_baseline.tsv"
    comments, header, rows = read_tsv(path)
    out = []
    for row in rows:
        us, _, status = t.functions.get((row["image"], int(row["va"], 16)), (None, 0, "removed"))
        if status != "exact":
            log[f"match_baseline.tsv unbanked ({status})"] += 1
            continue
        row["va"] = f"0x{us:08x}"
        out.append(row)
    write_tsv(path, comments, header, out)


CLAIM = re.compile(r'\b(ADDRESS|DATA|RODATA|SDATA)(_AT)?\(\s*(?:"(\w+)"\s*,\s*)?(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)')


def retarget_sources(t: Translator) -> None:
    for path in sorted((ROOT / "src").rglob("*")):
        if path.suffix not in (".c", ".h", ".inc", ".s"):
            continue
        default = SRC_IMAGE.get(path.relative_to(ROOT / "src").parts[0])
        text = path.read_text()

        def claim(m):
            kind, at, img, va, size = m.groups()
            image = SHORT[img] if img else default
            jp = int(va, 16)
            if kind == "ADDRESS":
                us, us_size, status = t.functions.get((image, jp), (None, 0, "missing"))
                if status in ("removed", "missing") or us is None:
                    unresolved.append(f"{path.relative_to(ROOT)}: {kind}{at or ''} {image} {va} has no US function")
                    return m.group(0)
                new_size = f"0x{us_size:x}" if status == "changed" else size
            else:
                us = t(image, jp)
                if us is None:
                    unresolved.append(f"{path.relative_to(ROOT)}: {kind}{at or ''} {image} {va} unresolved")
                    return m.group(0)
                override = t.data_overrides.get((image, jp))
                new_size = f"0x{override[1]:x}" if override else size
            log[f"src {kind} claims"] += 1
            whole, base = m.group(0), m.start(0)
            return (whole[:m.start(4) - base] + f"0x{us:08x}" + whole[m.end(4) - base:m.start(5) - base]
                    + new_size + whole[m.end(5) - base:])

        new = CLAIM.sub(claim, text)
        # other image addresses: comments are translated, code literals reported
        if default:
            def comment(m):
                return t.text(default, m.group(0))
            new = re.sub(r"/\*.*?\*/|//[^\n]*", comment, new, flags=re.S)
        for lineno, line in enumerate(CLAIM.sub("", new).splitlines(), 1):
            for m in HEX.finditer(line):
                unresolved.append(f"{path.relative_to(ROOT)}:{lineno}: literal {m.group(0)} (review)")
        if new != text:
            path.write_text(new)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--us-dir", type=Path, required=True)
    args = parser.parse_args()
    images = {k: UsImage(args.us_dir / v) for k, v in US_FILES.items()}
    t = Translator()
    retarget_functions(t, images)
    retarget_function_rows(t, "config/retail/functions_vendored.tsv", ("note", "evidence"))
    retarget_function_rows(t, "config/retail/function_identities.tsv", ("note", "evidence"))
    retarget_data(t, images)
    finalize_data(images)
    retarget_simple(t, "config/retail/data_identities.tsv", ("va",), ("note", "evidence"))
    retarget_simple(t, "config/retail/data_sections.tsv", ("va",), ("evidence",))
    retarget_simple(t, "config/retail/data_reservations.tsv", ("va",), ("evidence",))
    retarget_simple(t, "config/retail/link_labels.tsv", (), ("evidence", "note"), end_cols=("va",))
    retarget_simple(t, "config/retail/section_boundaries.tsv", ("first_right_start",), ("evidence",),
                    end_cols=("last_left_end",))
    retarget_simple(t, "config/evidence/psyq_30_complete_objects.tsv", ("va",), ("note",), drop_unresolved=True)
    retarget_simple(t, "config/evidence/psyq_30_text_sections.tsv", ("ram_va",), drop_unresolved=True)
    add_us_only_identities()
    retarget_relocs(t, images)  # after identities: scanned rows take names by US address
    retarget_baseline(t)
    retarget_sources(t)
    for key, n in sorted(log.items()):
        print(f"{n:6d}  {key}")
    print(f"\n{len(unresolved)} unresolved:")
    for line in unresolved:
        print("  " + line)


if __name__ == "__main__":
    main()
