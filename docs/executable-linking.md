# Source-to-EXE build

Inside `nix develop`, run:

```sh
kf build
```

`kf link` is an alias for the same executable builder. There is one candidate
artifact chain:

```text
C source -> CPPPSX/CC1PSX -> assembly -> ASPSX -> Psy-Q OBJ
Psy-Q OBJ + original SDK OBJ/LIB -> PSYLINK -> CPE -> CPE2X -> PS-X EXE
```

The EXE is the unchanged output of CPE2X. Retail bytes are comparison inputs
only. The build does not synthesize object sections, force addresses from
`DATA()` claims, rewrite the CPE, patch the EXE, or copy missing retail bytes.
A failed phase removes the stale EXE.

DOSBox-X runs every DOS tool at a fixed emulated rate (`DOS_CYCLES` in
`scripts/psxbuild/sdk.py`). CPE2X leaves its reserved header words
(`0x08..0x0f`) uninitialized, so they hold stack bytes that real-mode timer
interrupt frames wrote. With `cycles=max` the rate followed host load and those
bytes varied between otherwise identical builds; running OPEN's conversion at
500, 1000 and 3000 cycles per millisecond gave `a3 27`, `af fe` and `9a 0b`.
At a fixed rate the interrupt schedule depends only on executed instructions,
and `nix flake check` (`executable-determinism`) builds all four programs in
two trees and requires identical CPE and EXE bytes.

The pinned Psy-Q 3.0 kit supplies PSYLINK 1.29, headers, libraries, and the
overlay `NONE2.OBJ` startup. The converter is the CPE2X 1.3 build from Sony's
Runtime Library 3.0 CD (see [the header section](#ps-x-exe-header)). The
active chain still uses the separately hash-pinned ASPSX 1.07 inherited from
the SLPS-00017 setup; the kit's ASPSX 2.08 is not yet wired in. The build report records every tool and input hash. This is
a reproducible source-to-EXE chain, while exact historical compiler and
assembler attribution remains open.

GAME startup passes `BSS_END` to `InitHeap`. Retail loads it with a
relocated `lui`/`addiu` pair and subtracts it at run time, so the original
referenced a link-time symbol rather than a folded number (KF1's startup, by
contrast, used compile-time numbers). PSYLINK defines no section-end symbols
and an LNK XDEF is a section plus offset, so the label must come from an
object placed after `.bss`; `config/link/overlay_bounds.asm` stands in for
that unrecovered object with one zero-byte label. A separately pinned native
ASMPSX 2.34 assembles it because the C assembler lacks named sections and the
kit's ASMPSX 1.21 is not yet validated under DOSBox. No retail code uses a
`.bss` start label, so none is declared. OPEN and END pass numeric heap
bounds.

## SDK member order

PSYLINK, not the build script, places SDK library members. The build only
chooses the explicit objects and the `inclib` order (`LIBRARIES` in
`scripts/psxbuild/link.py`). Relinks of the same objects that change only
that order show these PSYLINK 1.29 rules:

- Archives are visited in `inclib` order. The groups of members from each
  archive follow one another in the linked text in that order.
- After it takes a member, the linker searches again from the first archive.
  A dependency in an archive listed earlier is placed directly after the
  member that needs it: in all three overlays LIBAPI's `GPU_cw` (C73) follows
  LIBGPU `SYS` and precedes `TMD`. A dependency in an archive listed later
  waits for that archive's group.
- Inside one archive, members follow the linker's own symbol-table order.
  This is neither archive order nor first-reference order. LIBAPI and LIBCARD
  hold byte-identical `C112` (`_bu_init`) members. Both retail and the relink
  put it in the LIBAPI group, although LIBCARD is visited first. These
  placements come from running PSYLINK, not from a model of it.

The retail placements in `config/retail/functions_vendored.tsv` fix the order:

| Image | Explicit objects after the game units | `inclib` order |
| --- | --- | --- |
| `GAME.EXE` | `NONE2.OBJ`, `MALLOC.OBJ`, `CARD.OBJ` | LIBSN, LIBCARD, LIBCD, LIBSPU, LIBSND, LIBGTE, LIBETC, LIBAPI, LIBC, LIBGPU |
| `OPEN.EXE`, `END.EXE` | `NONE2.OBJ` before the movie units; `MALLOC.OBJ` | LIBSN, LIBAPI, LIBC, LIBPRESS, LIBGPU, LIBGTE, LIBCD, LIBETC, LIBSND, LIBSPU |

The GAME evidence:

- Retail GAME text holds the member groups CARD, CD, SPU, SND, GTE, ETC, API,
  C, GPU.
- The SND members that call SPU sit inside the SPU group. For example,
  `VS_VTBP` is followed by its `S_STM`, `S_WP` and `S_GTSA`, and `UT_REV`
  by `S_SR`. So LIBSPU comes before LIBSND.
- No GAME member comes from LIBPRESS, so LIBPRESS is not linked.
- LIBSN supplies no member to an overlay and has no effect on the output.

`CARD.OBJ` (Psy-Q 3.0 `LIB/CARD.OBJ`) is linked explicitly. Retail places its
`_card_clear` directly after `MALLOC.OBJ`, and no GAME code calls it. Its
`_new_card` and `_card_write` references pull LIBCARD A80 and A78.
`MALLOC.OBJ` supplies the overlay `InitHeap`, `malloc` and `free` in place
of the LIBAPI/LIBC BIOS stubs, and it pulls LIBC `bcopy` and `bzero`.

With this order the relinked GAME text has the same 120 SDK member runs, in
the same order, as the retail inventory. Two inventory rows cover two members
each: `S_R|S_W` is the archive-ambiguous pair, and the `EVENT` row also covers
`96VEC`. OPEN and END already matched member for member. In the SDK text of
all three overlays, every differing word is a relocated field:

| Image | `jal`/`j` targets | `lui` | 16-bit low/offset fields | Other |
| --- | ---: | ---: | ---: | ---: |
| GAME (retail VA = candidate VA + 4) | 1249, each exactly 4 lower | 899 | 2078 | 0 |
| OPEN | 0 | 748 | 2019 | 0 |
| END | 0 | 865 | 1967 | 0 |

So no SDK member differs from the pinned archives. The GAME call targets
differ by the text's 4-byte start offset. The `lui` and low fields address
data whose placement is game-data work. In OPEN and END, retail puts the
game's small initialized block (0x34 and 0x18 bytes) at the end of `.data`,
just before `.sdata`; the candidate puts it at the start.

## PS-X EXE header

`KERNEL.H` defines the 0x88-byte header as follows:

- 0x00: the eight-byte key `PS-X EXE`.
- 0x08: two reserved words.
- 0x10: the fifteen-word `EXEC`: `pc0`, `gp0`, `t_addr`, `t_size`, the
  data and BSS extents, `s_addr`, `s_size`, and the save area `sp`, `fp`,
  `gp`, `ret` and `base`.
- 0x4c: a 60-byte title.

The rest of the 2048-byte sector is zero padding. The converter writes the
whole structure, so any field it does not store keeps whatever was on its
stack.

Two CPE2X 1.3 builds differ in their header writers:

| Converter | SHA-256 | Fields it stores |
| --- | --- | --- |
| Psy-Q 3.0 kit `BIN/CPE2X.EXE` | `8ee3df02…ef20` | key, `pc0`, `t_addr`, `t_size`, data/BSS extents, title |
| Runtime Library 3.0 CD (DTL-S2180) `PSXGRAPH/BIN/CPE2X.EXE` | `641d95eb…8af2` | also both reserved words, `gp0`, the save area and `s_size` as zero, and `s_addr = 801ffff0` |

`tests/test_cpe2x_header.py` runs both writers with two stack fills. The
Runtime build leaves exactly 0x7c–0x87 unwritten: the title tail after the
48-byte `Sony Computer Entertainment Inc. for Japan area` string. The
retail PSX, OPEN and END headers have this shape: zero reserved words,
`gp0` and save area, and `s_addr` 801ffff0. The kit build cannot write that
shape, so the build uses the Runtime build. Nix fetches the CD image from
Archive.org (`ps1_sdks`, SHA-256 `0717a820…ef37`) and extracts only this file.

Both builds leave the same kind of stack residue in the title tail:

| Offset | Retail PSX / OPEN / END | Meaning |
| --- | --- | --- |
| 0x7c | `0005` | saved word |
| 0x7e | `0fc2` | saved frame pointer |
| 0x80 | `158e`, then `3b30` / `3b30` / `36b0` | far return address of an earlier call: offset `158e`, then the converter's code segment |
| 0x84 | `0005`, `0504` | that call's arguments; the kit build's second argument is `04fe` |

Controlled conversions under DOSBox-X show what moves these words:

- **Frame pointer.** It moves with the length of the CPE file argument. The
  converter's startup puts the argument strings on the stack, rounded to a
  word.
  - Bare names give `0fca` for `PSX.CPE` and `END.CPE`, and `0fc8` for
    `OPEN.CPE` and `GAME.CPE`.
  - A six-character directory prefix gives `0fc4` for PSX and `0fc2` for
    OPEN.
  - A seven-character prefix gives `0fc2` for all three names.
- **Code segment.** It is where DOS loaded the converter. `LOADFIX -64`
  moves it from `0822` to `18ac`. DOSBox-X's `minimum mcb free = S` puts it
  at `S + 0x122`.

So the retail conversions passed a 15- or 16-byte argument and ran with the
converter's code at `3b30` (PSX, OPEN) or `36b0` (END). END was converted in
a different DOS session from PSX and OPEN. These are facts about the
historical DOS session. The residue records only the argument's length, not
its spelling, and not what occupied the roughly 237 KB (or 218 KB) of
conventional memory below the converter.

The build reproduces that session. It converts `CPEDIR\<NAME>.CPE`, a
seven-character prefix, for every image. It sets `minimum mcb free` from
`CONVERTER_CODE_SEGMENTS` in `scripts/psxbuild/link.py`. The converter still
writes every byte; nothing is patched afterwards. With this session the PSX,
OPEN and END headers equal retail, and `PSX.EXE` is byte-identical.
`NativeBuildControls` checks the residue on a synthetic program, so it does
not depend on the game input.

### GAME header

Retail GAME was converted by neither CPE2X 1.3 build. With the Runtime
build, 52 of its header bytes still differ:

- **0x10 (one byte).** `pc0` is `800498c4` in retail and `800498c0` in the
  candidate. This is the 4-byte game `.rodata` difference, not converter
  residue.
- **0x08–0x0f and 0x14–0x17 (12 bytes).** Retail has `56 44 dc ff 52 ff`,
  then `co`, then `t fr` in `gp0`. That is the text `convert from` with
  `pc0` written across its middle.
- **0x30–0x4b (20 of 28 bytes).** Retail has binary residue in the stack and
  save-area fields. The Runtime build writes these fields.
- **0x7c–0x8f (19 of 20 bytes).** Retail has
  `04 02 00 00 04 02 19 13 00 02 ea 01`, then the first eight bytes of a CPE
  file: `CPE\x01`, select unit 0, and a `pc` register record.

The 20 bytes at 0x7c–0x8f, `dc ff 52 ff 63 6f` at 0x0a and `t fr` are
identical in all three King's Field (SLPS-00017) retail headers. In those
headers 0x30–0x4b held linker-map text. GAME was therefore converted by the
same older converter and process as the 1994 game. Both CPE2X 1.3 builds
clear 0x88 onwards from a global buffer, so neither can write the CPE prefix
at 0x88.

That converter is not in the Psy-Q 3.0 kit, the Release 2.5 floppies, or the
Runtime Library 2.0, 2.6 or 3.0 CDs. It is
probably CPE2X 1.2 or earlier: the Runtime CD's notes say 1.3 changed only
the handling of unconvertible CPE files. A candidate is identified by these
51 bytes from GAME's own CPE.

Derived ELF objects, delinked retail modules, objdiff projects, and semantic
reports are analysis views. Source compilation in `kf analyze` and `kf try`
uses the same CPPPSX/CC1PSX/ASPSX implementation as `kf build`. The reader
translates the resulting native LNK objects for objdiff; no GNU assembler
recompiles game source and no ELF view is an input to PSYLINK.

ASPSX can erase an in-object data symbol into a section-relative reference.
In SLPS-00017, `floor_entry_cells-2` became `.data+14` when the table started
at `.data+16`.
The reviewed `config/native_reloc_referents.tsv` records the image, function,
exact function-relative HI/LO offsets, data owner, signed addend and evidence
for exceptions that need their named owner in the comparison view. The
SLPS-00069 table has no rows yet.

The reader applies a row only to its exact pair. Both native patches must
refer to the same section and address; the named owner must be uniquely
defined there, and `owner offset + addend` must equal the original native
target. The instruction pair must be LUI plus a sign-extending low operation
using its register. Missing, moved or contradictory pairs fail instead of
choosing another symbol. Identical addresses at other sites remain unchanged.
No assembly-wide spelling search or nearest-owner selection is performed.

This follows Gruntz's explicit referent-table approach on the native-view side.
Retail owner evidence remains independently curated in `config/retail/relocs.tsv`;
the native conversion does not read a retail executable or borrow its bytes.
Tests reconcile each current manifest row with that independent retail record.
The TSV is a direct compile dependency and applied rows are recorded in object
metadata. Only the ELF view's symbol/addend split changes; native LNK objects,
PSYLINK inputs and executable bytes remain unchanged. These rows are reviewed
reconstruction evidence, not proof of historical source spelling.

The shared probe enables native compiler/assembler debug metadata for private
symbols and function records. The source assembly passes unchanged except for
DOS line endings. Native COMMON reservations remain unplaced in the ELF view;
only PSYLINK assigns their executable addresses.

Current source-to-EXE status is distinct from exactness and playability:

| Property | Current status |
| --- | --- |
| PSYLINK emits PSX, GAME, OPEN, and END | Wired through `kf build`; needs at least one source unit per image |
| Candidate artifact path | Direct source-to-EXE with no output rewriting |
| Historical toolchain identity | Open; the usable ASPSX is separately sourced |
| Byte-identical retail images | `PSX.EXE`; OPEN and END headers only |
| Boot | Not yet attempted for SLPS-00069 |

An SDK mismatch does not by itself prove that an executable is unplayable: a
different library revision may preserve the public API and runtime behavior.
It also cannot establish complete playability, because private object layouts,
data, callbacks, and initialization behavior can differ even when linking
succeeds. The original hash-verified retail disc remains the runtime reference.

## Game data placement

Each object contributes one run per output section in link order, so a
datum's section and its unit's link position fix its address. Facts that
decided the current GAME, OPEN and END layouts:

- GCC 2.5.7's C front end emits every initialized file-scope or local static,
  const or not, at its declaration even when nothing reads it, and keeps
  unused non-const tentative statics as `.lcomm` requests. The images retain
  such leftovers (the word after the map-object jump tables, the bytes after
  the identity matrix, the eleventh menu label row, KF1's palette rectangles,
  small-data words and four card `.sbss` slots); each is a curated
  unreferenced identity rather than padding.
- A jump table is emitted after `.align 3`, which ASPSX applies relative to
  the object's `.rdata` start; table offsets therefore show which object an
  unexplained word belongs to.
- Under `-G8`, initialized data of at most eight bytes goes to `.sdata` and
  private tentative data of at most eight bytes to `.sbss`; exported COMMON
  always stays in `.bss`. OPEN's and END's small game globals, and the movie
  unit's tutorial statics, are therefore `-G8` objects.
- PSYLINK places fixed `.bss` reservations in link order, then allocates
  exported COMMON by symbol-hash bucket, `(len + byte sum) & 511`, as decoded
  in the KF1 project. The native build follows that order exactly, so a game
  COMMON object reaches its retail address only if its source name falls in
  the bucket interval its SDK neighbours bound. `kf common-order` reports
  which curated names do not. Curated identities only bound that interval
  loosely: unidentified SDK COMMON (from the linked `.SYM`) tightens it, and
  retail may hold game COMMON nothing references (US END keeps one 8-byte
  slot before its screen buffers). Names chosen to fit are WIP spellings,
  not recovered ones; a shared OPEN/END source spells per-image names under
  `KF_END` where the two intervals do not overlap.
- `kf verify data` accepts a unit's COMMON placement only when the current
  link's `.SYM` (`scripts/kf/linked_symbols.py`) puts every request at its
  claim, and the link report's input hashes all match the working tree.
- The US PSX, OPEN and END headers name the "North America area"; both
  available CPE2X builds write only the Japan marker, so those 20 header
  bytes differ until the US converter is found.

### Runtime smoke test

`kf-run-candidate` substitutes linked executables into a copy of the raw retail
Mode 2 disc (`KF_CANDIDATE_IMAGES` selects which ones, for controlled hybrid
tests). The retail BIN must match the SLPS-00069 SHA-256
`ca7d6d616d125c2d7bd7be131e8362eddb0b71cd15a2902dae73453dbe043a31`. Every
changed sector's EDC and P/Q ECC is regenerated; `kf-run-retail` boots the
unmodified disc.

Existing source-to-EXE outputs can be compared again without rebuilding them:

```sh
kf link --compare-only
```

The PSYLINK controls use original Psy-Q objects and libraries to test section
alignment and BSS allocation.

## Layout-tolerant executable comparison

`kf link --compare-only` compares moved byte regions in existing outputs and
refreshes the second generated README block, `executable-score`:

```sh
kf link --compare-only
```

The command accepts `--image psx|game|open|end`. The README keeps one row per image
and a byte-weighted total for the available images. It reads saved reports
only when their candidate hashes match EXEs still on disk and their retail
hashes match the configured retail identities. Missing, changed or failed
outputs receive no cached score. The executable and function blocks share
the same update lock and preserve each other.

The **Data modules exact** column shows complete passing source data owners
out of all compared owners, using the same strict gate as `kf verify data`.
A module counts only when every initialized section, relocation, BSS layout
and retail section placement passes. Exact functions or equal initializer
bytes alone do not make its data exact. Separate config-owned SDK contributions
are excluded; these object checks do not measure the generated EXE's similarity.
`kf analyze`/`kf check` and `kf bank` also refresh this column. Missing comparison
artifacts or objects older than their source, headers, inventories or build
inputs show `—`, independently of executable-score availability.

The report is a content heuristic called `unique-byte-islands-v1`, not a
reimplementation of objdiff or a claim of function identity:

1. Find exact 16-byte windows that occur once in each load area. Coalesce
   overlapping windows with the same displacement, then select the longest
   nonoverlapping runs. Each byte can be assigned at most once in each image.
2. Extend selected runs through equal, unused neighboring bytes. Also keep
   exact file-boundary prefixes and suffixes, so repeated padding does not
   require a unique seed.
3. Join adjacent anchors only when they are neighbors in both images. Each
   intervening gap is at most 128 bytes. A bounded byte-sequence comparison
   accepts gaps with at least 60% similarity, or edits of at most 16 bytes
   per side when the two flanking exact runs supply at least 85% similarity
   across that local span. Unchanged bytes inside an accepted gap earn
   credit; its edited, inserted or deleted bytes do not.
4. Sum equal byte pairs across all selected islands. Report similarity as
   `200 * equal_pairs / (retail_load_bytes + candidate_load_bytes)`. All
   unpaired bytes remain in the denominator. The nonzero score uses the
   same formula with zero-valued bytes removed from both counts.

Islands may move or reorder, but no address, opcode, register, immediate,
constant or data byte is masked. A changed pointer still loses byte credit;
similar pointer encodings do not prove the same referent. Short or repetitive
regions without suitable anchors can remain unpaired even when shared.
The algorithm is conservative and deterministic, rather than an optimal
global alignment or an instruction-semantic comparison.

The island scores cover the complete EXE load areas, including sector
padding, and exclude the 2048-byte headers. The separate fixed-offset
comparison still covers the complete files and is the only executable
equality check. Neither island similarity nor island coverage changes
function scores, banking criteria or the full data-layout gates.

Normal builds write tool commands and input hashes into each image's
`build.json`. They do not load retail EXEs, compare output bytes, or update
the README. Explicit comparison-only runs write `fuzzy-comparison.json` and
`fuzzy-comparison.html`, preserving the build report and executable.
The standalone HTML contains a movement
map, filterable islands with both addresses and sizes, and the largest
unanchored regions. The JSON retains all islands, complementary unanchored
ranges, parameters, counts and input hashes. These generated files stay
under `build/link/{psx,game,open}/` and are not committed.

## Verification and limits

The full rebuild compared 484 functions, including thirteen source-verified vendor functions. Every score is unchanged from the pre-edit snapshot: 460 remain exactly 100%. The game-only totals remain 340/362 GAME, 106/108 OPEN and 1/1 PSX. No new function was banked.

All 22 newly defined initialized objects match their retail extents and raw bytes in the freshly compiled objects. This includes every sound selector, the six light matrices, path arrays and initialized state words; it does not establish their linked positions.

At the time of that experiment, the full repository suite passed all 739 tests
with local retail/build inputs. `ruff check scripts tests`, `git diff --check`
and `nix flake check -L` also passed. Those counts record the retired campaign,
not the current suite.

The executable-island controls cover reordering, small edits, insertions/deletions, duplicate-copy accounting, unrelated content, padding, changed address encodings and malformed EXEs. README controls cover hash freshness, weighted totals, preservation of the function block and repeatable updates. The source-to-EXE build and a subsequent comparison-only pass gave identical island reports for all three KF1 images; the latter left native EXEs and build-provenance reports unchanged. These integration checks are recorded under `build/link/island-audit/`.

The native executable controls check the minimal startup opcodes and
symbolic entry, all four BIOS aliases, initialized zero data, exclusion of an
8192-byte BSS buffer, unchanged original archives, and rejection of unresolved
references or stale output. They do not execute a complete game image.

The existing function oracles now explicitly opt into named-object data bindings where merged modules own data from separate retail regions. Each object keeps its compiled extent and initializer; ambiguous section-relative references, unknown objects, wrong extents and overlapping initialized objects are rejected. The returned program records `data_binding=objects`. This isolates function semantics and does not prove section placement. Default oracle linking still requires a consistent section base. This facility is not used by `kf link`, the delinker or the full data-layout checks.

`kf analyze` still exits unsuccessfully on data layout and incomplete ownership.
With native LNK-derived views it reports 0/1 PSX, 9/20 OPEN and 23/41 GAME
data-owning units matching. Unplaced COMMON reservations remain explicit
failures rather than receiving inferred section offsets. Equal bytes or
unchanged resolved functions do not waive ownership and placement failures.
Earlier closure reports remain under `build/link/overlay-link-audit/`; current
native build commands and input hashes are in each image's `build.json`.
