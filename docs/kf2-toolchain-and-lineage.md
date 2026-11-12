# King's Field II: SDK, compiler, function census, and regional lineage

First-pass evidence for the three King's Field II builds. Everything here is
reproducible from `scripts/` against the executables in `retail/` (ignored;
copied from `kings-field-investigation/extracted/`). Function counts are
machine seeds, not a curated inventory.

| Key | Serial | Boot EXE | Role |
| --- | --- | --- | --- |
| `jp` | SLPS-00069 | `PSX.EXE` | Japanese original, primary target |
| `us` | SLUS-00158 | `SLUS_001.58` | US localization ("King's Field") |
| `eu` | SCES-00510 | `SCES_005.10` | PAL En/Fr/De localization ("King's Field") |

All three discs use a 2 KiB boot loader at `0x80010000` plus three overlays
that share one window at `0x80011000`: `OPEN.EXE`, `GAME.EXE` and `END.EXE`.

## 1. Sony runtime library (SDK)

### Direct RCS witnesses

| Build | `$Id$` strings in OPEN/GAME/END |
| --- | --- |
| jp, us | `intr.c` 1.52 1995/03/14, `pad.c` 1.33 1995/03/14, `vsync.c` 1.7 1995/03/14, `sys.c` 1.67 1995/03/13, `s_crwa.c` 1.8 1995/03/11 |
| eu | `bios.c` 1.71 1995/12/01, `intr.c` 1.73 1995/11/10, `sys.c` 1.116 1995/12/01 |

JP and US link the identical March 1995 runtime snapshot. EU was relinked
against a runtime from December 1995.

### Object-signature census (`scripts/psyq_sig_census.py`)

Every object signature from the `ghidra_psx_ldr` Psy-Q corpus was searched,
with relocation bytes wildcarded. The corpus covers SDK versions 2.6, 3.0, 3.3,
3.4, 3.5, 3.6.10, 3.6.11, 3.7 and 4.0 through 4.7. The table shows the share
of matched library bytes by version. Only objects of at least 64 bytes count.

| Library | jp/us GAME | eu GAME |
| --- | --- | --- |
| LIBCD | **3.0 100%** (2.6 30%) | **3.4 99%** (3.3 47%) |
| LIBETC | **3.0 100%** | **3.4 100%** (3.3 95%) |
| LIBGPU | **3.0 100%** (2.6 80%) | **3.4 99%** (3.3 33%) |
| LIBGTE | **3.0 92%** | 3.3 88% / 3.4 86% |
| LIBSND | **3.0 100%** | **3.4 100%** |
| LIBSPU | **3.0 100%** | **3.4 90%** |
| LIBPRESS (OPEN/END) | none matched | **3.4 100%** |

- **JP/US: Psy-Q / Runtime Library 3.0.** This is proven. The archived 3.0 kit
  (below) contains exactly the five RCS revisions listed above.
- **EU: Runtime Library 3.4.** This is strong. A few GTE objects match only
  3.3 or 3.5+ signatures. That could be real mixing (for example a GTE
  library from another drop) or corpus splitting noise. It needs the actual
  3.4 `.LIB` files to settle.
- `MALLOC.OBJ` matches the 2.6/3.4 signature in every build. `PSX.EXE` holds
  only `2MBYTE.OBJ`-style startup, which is identical across all versions.
- JP/US `OPEN.EXE` and `END.EXE` contain Sony code that the 3.0 corpus does not
  cover, presumably the MDEC/STR movie path. It lies inside the SDK span and is
  counted as SDK below.

## 2. Compiler and assembler

A PS-X EXE carries no compiler banner, so attribution comes from codegen
fingerprints (`scripts/codegen_fingerprints.py`). They are split between
signature-matched Sony bytes and everything else.

| Fingerprint (game code) | KF1 SLPS-00017 | jp | us | eu |
| --- | --- | --- | --- | --- |
| Framed epilogue: `addiu sp` in the `jr ra` delay slot | 370/375 | 252/256 | 252/256 | **0/152** |
| Framed epilogue: `addiu sp` before `jr ra`, `nop` slot | 3/375 | 3/256 | 3/256 | **152/152** |
| `$gp`-relative loads/stores (small data) | 1 | 0 | 0 | **408** |
| `div` with `break 7` / `break 6` traps | yes | yes | yes | yes |
| `$fp` frame setup (`-O0` sign) | 0 | 0 | 0 | 0 |

(EU counts are over Ghidra-seeded game functions only. The Sony runtime
objects that still use the old epilogue are excluded.)

- **JP/US game code** was built with the same compiler class as KF1: an
  optimizing GCC that places the stack restore in the return delay slot, with
  `-G0` (no small data). The KF1 probe campaign showed that GCC 2.6.0 cannot
  emit that epilogue, while 2.4.1 and 2.5.7 do. The Psy-Q 3.0 kit ships both
  `CC1PSX` 2.4.1 and `CC1PSX.EXE` 2.6.0, byte-identical to the Release 2.5
  pair already in the KF1 toolchain. So **GCC 2.4.1 from the kit is the
  natural candidate**, with ASPSX 2.08, CCPSX 1.10 and PSYLINK 1.29. This is
  not yet proven by a compile probe.
- **EU game code was recompiled, not relinked.** No framed EU game function
  keeps the old epilogue, and `$gp` small data appears (`-G` > 0). The
  contemporary SN compiler is `CC1PSX.EXE` **GCC 2.7.2.SN.1** (dated
  1995-12-20). It ships with ASPSX 2.34 (1996-02-20) and CCPSX 1.18
  (1996-02-14). It's a candidate, not proven: GCC 2.6.x would also produce
  the new epilogue.

### GCC 2.5.7 mechanisms behind source shape

Matching against the `cc1psx-257` probe (`-O2 -mcpu=r2000`) traced these
source-to-codegen links in the 2.5.7 sources and RTL dumps (`-dr -dL -dl
-dg -dS`). They explain retail differences without a compiler change.

- **Constant addresses (expr.c, explow.c).** A field read whose value is
  used directly (`p = DISPLAY.primitive_buffer;`) goes through
  `change_address`/`memory_address`, which forces the constant address into a
  pseudo for CSE. The pointer operand of `->` is expanded with `EXPAND_SUM`
  and builds `(mem (const ...))` without that pseudo. Both emit the same
  `lui`/`lw`, but only the forced pseudo becomes a loop movable.
- **Loop invariant hoisting (loop.c `move_movables`).** Threshold =
  (call in loop ? 1 : 2) x (1 + 25 non-fixed GPRs) and drops by 3 for each
  register moved. An invariant moves only if threshold x savings x lifetime
  >= the loop's real insn count, which includes the `(use aN)` insns before
  calls. Movables are tried in insn order. An extra address pseudo early in
  the loop can therefore keep a later constant from being hoisted
  (`menu_fade_transition`). An invariant that is used only once in a loop
  with calls is substituted back into its use and costs nothing.
- **CSE paths (cse.c `cse_end_of_basic_block`).** A path runs on through a
  conditional branch around a block, so `x = a; if (c) x = b;` keeps earlier
  constant pseudos live. That can push a shared `-1` into a saved register
  across calls. An `if/else` whose arms end at a common join label stops the
  path. Calls duplicated in both arms are cross-jumped back into one later
  (`render_world_model`). A store to a global between a field store and its
  re-read invalidates the memory equivalence and forces a reload.
- **Strength reduction (loop.c).** Threshold = (call ? 1 : 2) x (3 + 25). A
  giv is reduced only if lifetime x threshold x (benefit - add cost) >= insn
  count. Identical givs combine, adding lifetimes and benefits. A
  non-replaceable user-variable giv also pays a copy cost. A pointer the
  source advances by hand, initialized before the loop, is set up ahead of the
  reduced givs. Reduced givs are set up and incremented in reduction order.
- **Register priority (local-alloc.c, global.c).** Priority =
  floor_log2(refs) x refs / (live length x words). Ties go to the lower
  pseudo number, so declaration order decides them. References inside a loop
  count `loop_depth` times. A `do { } while (0)` contour or the copied entry
  test of a `for`/`--n` loop therefore raises the weight of the uses in it.
  `update_equiv_regs` doubles the live length of single-set constants and of
  unmodified stack parameters. Reusing one local for two values merges their
  live ranges. Local-alloc gives callee-saved registers to single-block
  pseudos that cross calls before global-alloc runs. A block local in `s2`
  therefore moves a global to `s3`.
- **Caller-save (caller-save.c).** A call-clobbered register is kept across
  calls only if refs > 4 x calls crossed.
- **Scheduling (sched.c, backward list).** Priority is the longest latency
  path. An insn that births a register gets `0x7f000001` once ready. Ties
  then go by class relative to the last scheduled insn, then by highest LUID.
  In block 0 the leading hard-register parameter copies are pinned until the
  first non-copy, so a parameter copy that combine has deleted changes which
  copies the scheduler may move. A read after a non-const call depends on
  that call.
- **Stack frame (function.c `assign_stack_temp`).** Block-scoped aggregates
  reuse a free slot of the same size first, most recently freed first.
  Otherwise they split the smallest larger slot in 8-byte units, and
  adjacent free slots merge at the end of each statement or block. Block
  nesting and declaration order therefore decide aggregate offsets. An
  address-taken scalar gets a permanent slot and stays in memory. A union of
  `u32`, `s16[2]` and `u8[4]` gets SImode and lives in a register. A pseudo
  whose insns combine deleted can still get an 8-byte reload slot at the top
  of the locals.
- **Delay slots (reorg.c).** A redundant insn at a branch target is skipped,
  so the slot is filled from the fall-through. A target insn is not copied
  when its destination is live on the fall-through.
- **Hoisted copies and cross-jumping (loop.c, combine.c, jump.c).** Loop
  invariants land after the copied exit test, so retail's preheader shows
  which values the loop body computed. Identical single-set invariants
  combine into one register; an `s16` local set from the same value is
  instead a separate movable whose `sign_extend` combine reduces to
  `move`, which is how one `andi` feeds two registers
  (`collision_evaluate_shape_records`). Jump2 cross-jumping deletes the
  earlier of two identical tails, so shared code that retail keeps in the
  first switch arm was a `goto` target in the source.
- **Stack arguments read in place (function.c `assign_parms`).** A
  parameter whose address is taken gets no pseudo, so `va_start (ap, kind)`
  keeps `kind` in its incoming slot and every use reloads it; the unnamed
  arguments after it are read through the folded cursor
  (`floor_item_capture_image`: `lbu`/`lw 56`, `lw 60`, `lhu 64`). A
  cursor set to `&effect_id` and advanced before each read
  (`*(T *)(ap += 4)`) anchors at the first argument itself
  (`player_dispatch_magic_effect`: `addiu s0,sp,112`, `lw 4(s0)`). Whether
  that function's first `effect_id` read is forwarded from `a0` follows the
  CSE hash-staleness rule below: forwarded at 305 or 307 pseudos, reloaded
  as in retail at 308.
- **Other folds.** Combine's nonzero-bits tracking covers only pseudos set
  once, so `x = (x << 8) >> 12` on a reassigned variable stays `sll`/`sra`.
  Reading a bitfield of a word defeats CSE against a plain read of that word.
  `p + i * size` expanded as a value keeps the pointer first in `addu`. The
  `EXPAND_SUM` address path puts the product first. `expand_binop` keeps a
  register first operand, so `&objects[i]` on a register local is
  pointer-first. A dead read through `objects[i]` just before it builds the
  sum on the address path, and CSE then reuses that sum
  (`event_world_dispatch_interaction`).

Lane B5 traced these further links:

- **Narrow parameters (function.c, local-alloc.c).** A promoted `u8`/`u16`
  parameter is copied from its argument register into a word pseudo. That
  copy is pinned at the top of block 0. The narrow variable itself starts at
  the later subreg copy, which sched1 may move down, so its live range is
  shorter than that of an `s32` parameter (`render_animated_object`). The
  parameter REG_EQUIV note, and with it the live-length doubling, applies
  only when the declared and passed modes match. A 32-bit stack parameter is
  doubled and a promoted narrow one is not.
- **Value reads of constant addresses.** `cur_avatar_status.camera_position.vx`
  used as a value goes through `memory_address`. That forces the address into
  a CSE-shared pseudo, which loop.c hoists in insn order. A pointer local set
  before the loop instead fixes the order by source position
  (`render_scene_and_update_resources`).
- **SDK macro arms.** `setSemiTrans(p, abe)` with a non-constant `abe` keeps
  the `getcode(p) & ~2` store in its clear arm. CSE resolves that store to the
  register that holds the packet code. The constant pseudo then lives into a
  second block and goes to global allocation, which shifts every later
  caller-saved choice (`render_textured_quad`).
- **Scheduler memory dependences (sched.c).** Two references conflict unless
  their constant offsets from the same base differ. There is one exception:
  a non-`QImode` `MEM_IN_STRUCT_P` reference through a varying address does
  not conflict with a non-struct reference at a fixed address. Component,
  array and address-sum references are "in struct". `*(u32 *)&v.vx` and
  `*ptr` are not. A struct's byte stores never get the exemption. Once more
  than 32 memory references are pending, the next store flushes them, and
  every later reference depends on that store. This includes a caller-save
  restore, which `save_call_clobbered_regs` inserts just before the first
  use. The memory form of a copy therefore decides how far restores and
  stores can float (`tmd_prepare_subdivided_object`).
- **Cross-jumping after sched2.** sched2 runs before jump2. Identical arm
  tails are therefore scheduled separately, with the join label as a block
  boundary, and merged afterwards. Duplicated calls or stores in both arms can
  explain an argument move that sits in a delay slot ahead of stores
  (`map_object_spawn_effect`).
- **Load-delay fillers.** When a load's consumer must wait, the scheduler
  fills the gap with the ready instruction that has the highest LUID. An
  independent statement moved into a call's region becomes that filler. This
  changes which argument-setup instruction wins the tie
  (`event_target_stream_execute`).
- **Reused locals and block-local combine.** Only a pseudo set once gets
  sched1's birthing promotion. A variable reused for several values therefore
  keeps its insns in source order. Combine works within one block, so a test
  placed after a join keeps a call-result copy in each arm, and jump2 merges
  the arms later (`effect_update_dispatch`).
- **Field of a declared variable vs cast view.** A field read from a declared
  aggregate global loads the whole constant address into a pseudo that later
  reads share. A cast-pointer base plus a field offset folds back to a direct
  `lui`/`lw`.
- **Dead sign-extension slots.** When a narrow signed field is compared and
  its loaded value is reused, a dead extension temporary is left behind, and
  reload gives it its own 8-byte frame slot.
- **CSE hash staleness.** A register that gets a new quantity leaves older
  table entries in stale buckets. Whether a later lookup finds them depends on
  the function's pseudo count modulo 31, so an unrelated edit can switch
  sharing on or off elsewhere.

Lane B6 traced these further links:

- **Macro contours and the log2 term.** Local-alloc priority uses
  floor_log2(refs), so one more loop level does not scale every quantity
  alike. Inside a loop, a `do { } while (0)` macro raises a three-use
  temporary from 6 to 9 weighted refs (priority 3.0 to 6.75) but a long
  tied quantity only from 16 to 24 (3.6 to 5.3). The temporary then takes
  `v0` first (`menu_draw_string`'s glyph-cell macro).
- **Three-quantity blocks (local-alloc.c `block_alloc`).** With exactly
  three local quantities, the sorting network's last step compares
  quantities 0 and 1 by number, not by position. The first-born quantity can
  therefore be allocated first despite a lower priority. A `divmodsi4` births
  its remainder quantity before its quotient.
- **Preferences from local copies (global.c `set_preference`).** A global
  variable copied from a local temporary inherits that temporary's hard
  register as a copy preference. `x /= 10` after a block-local `x % 10`
  keeps the quotient in a temporary until the copy, so the variable prefers
  the temporary's register over a parameter's (`memory_card_write_title_stats`).
- **Dead reload slots come from combine splits (combine.c `try_combine`).**
  Combine zeroes the use count of the middle insn's register only when the
  result is a single insn. When it splits the result in two (`newi2pat`) and
  that register disappears, its stale count makes reload give it an 8-byte
  frame slot. The compare is not essential to the narrow-field case: any
  sign extension of a narrow field whose narrow register is reused qualifies,
  for example `a = rec->pad; rec->pad += d;` (no visible `move` remains;
  `effect_update_dispatch`'s fifth slot). Other sources are the copied entry
  test of `for (i = 0; i < n; i++)` and of `for (x = n - 1; x != -1; x--)`
  over a parameter.

Lane A5 traced these further links:

- **CSE folding to absolute addresses (cse.c `fold_rtx`).** When a CSE path
  knows a pseudo's constant value, `(plus reg c)` addresses fold to absolute
  constants, also on a path that follows a jump into an else arm. A local
  copy of a stored offset let cse1 rewrite the else arm relative to that
  pseudo, and cse2 then made those stores absolute; reading the field back
  keeps both arms on one base register (`build_camera_map_cell_layer_masks`).
- **Entry-block locals seed `t0`.** A value used only in the entry block and
  copied into a loop counter is local; with `v0`-`a3` busy over its range it
  gets `t0`, and the copy gives the counter a `t0` preference that overrides
  the global priority order (`map_cell_add_layer_occupancy`).
- **Set-once birthing.** Writing `limit = a; limit += b;` instead of one
  assignment removes sched1's birthing boost and keeps the source load order
  (`collision_evaluate_shape_records` wall limit).
- **Constant reassociation (fold-const.c).** `a + (b - C)` and `C - a - b`
  are reassociated around the constant, which also decides where a hoisted
  invariant such as `0x800 - radius` is first met and so its preheader slot.
  Holding `b - C` in a local keeps `a` first in `addu`.
- **jump.c if/else rewrite.** `if (c) x = a; else x = b;` becomes
  `x = b; if (c) x = a;` only when the then-arm is one set. A then-arm such as
  `scale = value = 4096;` keeps the if/else, and reorg steals the else copy
  into the delay slot (`player_update_frame` overlay clamp).
- **Spill slots and chained clears.** Pseudos without a hard register get
  stack slots in ascending pseudo number, so declaration order fixes spill
  offsets; `a = b = c = 0` stores right to left (`player_move_horizontal`).
- **Block-local case variables.** A pointer or fraction declared per switch
  case stays single-block, so local-alloc ties its computing temporary into
  the callee-saved destination; declared at function scope it is global and
  cannot tie (`player_update_frame`).
- **Reference counts from reuse.** Storing further call results in an
  existing local, or wrapping a flag's test and set in `do { } while (0)`,
  raises its weighted references enough to reorder callee-saved choices
  (`player_update_vertical_motion`, `player_move_horizontal`). An in-place
  field update (`field += 10`) leaves the combine-deleted pseudo that
  explains an otherwise unreferenced 8-byte reload slot.
- **Loads held below a store (sched.c).** When the r2000 memory unit stalls
  loads behind a later store, the backward scheduler fills that cycle with
  any other ready insn, and fixed frame addresses never conflict. A load
  that writes the register the store reads (a reused variable) keeps the
  store above it; variables set more than once also lose the birthing
  promotion (`player_dispatch_magic_effect` case 3).
- **Reference counts precede combine.** flow counts a local's references
  before combine folds sets and tests away, so a shared local used by an
  unrelated path keeps a high global priority; reading the field directly
  there lowers it. One block-local reused for copied x and z fields keeps
  both loads in source order and in one register
  (`player_update_weapon_attack`).

The readability pass traced these source shapes:

- **Switch compare trees (stmt.c `balance_case_nodes`).** A small switch
  compiles to a balanced compare tree over its case values sorted in order.
  The pivot is node (n + ranges + 1) / 2 - 1. A range is tested as `< low`
  then `<= high`, and the cost table applies only when every value is
  printable ASCII. A goto chain that tests one value against constants in
  that order was a switch. An empty case still counts as a node: the
  FALLING pivot in `actor_update_vertical_motion` needs the SUSPENDED case,
  and the special-weapon test in `player_update_weapon_attack` is
  `case 16: case 17:`.
- **Range folds (fold-const.c).** `x >= a && x < b`, and two equality tests
  on adjacent constants, compile to the unsigned subtract-and-compare form.
  A `(u32)(x - a) < n` spelling was therefore the compiler's fold, not source.
- **Jump threading.** A `break` that lands on another `break` or `continue`
  threads into one jump. So does a goto to the label right after the
  enclosing loop or switch. `for` loops with `continue` and `break`
  therefore reproduce hand-written `next:` tails
  (`render_scene_and_update_resources`, `event_target_stream_execute`).
- **Cross-jumped single calls.** A call repeated in several arms that end at
  a common join merges back into one (`actor_play_target_sound`). A goto to
  a one-call block can therefore be written as the call itself.
- **Backward gotos are not loops.** loop.c sees only loops opened by `for`,
  `while` and `do`. A `retry:` label keeps its body out of loop optimization:
  `memory_card_read_slot` keeps a second register copy of `slot`. Retry
  labels therefore stay.

`docs/retained-gotos.tsv` lists the gotos that remain, their form and the
rewrites that were tested and rejected.

## 3. Function counts (Ghidra 12 + ghidra_psx_ldr seed)

Psy-Q links game objects before the libraries, so everything from the load
address up to the first matched Sony object is game code. The SDK span runs
from there to the last Sony object.

| Image | jp game | us game | eu game | jp SDK span | eu SDK span |
| --- | ---: | ---: | ---: | ---: | ---: |
| boot EXE | 1 | 1 | 1 | ~5 | ~5 |
| `OPEN.EXE` | 26 (8.4 KB) | 27 (10.7 KB) | 28 (9.0 KB) | 282 (63 KB) | 682 (92 KB) |
| `GAME.EXE` | **488 (217 KB)** | 489 (220 KB) | 494 (227 KB) | 330 (70 KB) | 776 (104 KB) |
| `END.EXE` | 17 (2.9 KB) | 14 (2.1 KB) | 14 (2.1 KB) | 262 (61 KB) | 501 (51 KB) |
| **Total game** | **532** | **531** | **537** | | |

- The reconstruction target is about 530 game functions and 230 KB of game
  code, almost all in `GAME.EXE`. `OPEN.EXE` and `END.EXE` are thin drivers
  around Sony movie and audio code. KF1's curated inventory, for comparison,
  has 935 GAME and 665 OPEN rows including SDK.
- The SDK-span counts are **not comparable** between JP and EU. Ghidra applied
  its 3.4 signatures to EU and split those libraries into named functions, but
  it recognized almost none of the 3.0 objects. Use bytes, not counts, for the
  SDK.
- KF1 found customized, version-skewed Sony code (LIBSND `SEQREAD`) inside its
  "game" span. The same could be true here and needs a skew-tolerant pass
  before the game count is final.

## 4. Similarity between the three builds

`scripts/function_similarity.py` compares game functions. **exact** means
identical once `jal`/`%hi`/`%lo`/`$gp` fields are masked. **shape** keeps only
opcodes and ignores registers, immediates and nops, so it survives a
recompile. "Similar" means the best 4-gram Jaccard score is at least 0.5.

`GAME.EXE`, JP as the reference (488 functions, 217 KB):

| Pair | exact identical | exact similar | shape identical | shape similar | unmatched (shape) |
| --- | --- | --- | --- | --- | --- |
| jp to us | **420** (155 KB) | 41 (43 KB) | 432 | 43 | 13 (7 KB) |
| jp to eu | 82 (7 KB) | 102 (52 KB) | 86 | 148 (114 KB) | 254 (96 KB) |
| jp to KF1 | 32 (2 KB) | 17 (3 KB) | 45 | 25 (6 KB) | 418 (208 KB) |

Best shape score per JP function, by share of bytes:

| Score | jp to eu | jp to KF1 |
| --- | ---: | ---: |
| ≥ 0.7 | 14% | 2.5% |
| 0.5–0.7 | 42% | 1.7% |
| 0.3–0.5 | 29% | 6.8% |
| < 0.3 | 15% | 89% |

`OPEN.EXE` jp to us: 17 of 26 identical. jp to eu: 2 identical, 23 unmatched.
`END.EXE` jp to us: 8 of 17 identical. jp to eu: 2 identical, 15 unmatched.

- **US is a localized relink of the JP code.** It has the same compiler and
  SDK, about 86% of functions are byte-identical after masking, and most of the
  rest are near-identical (text and layout edits). Mostly shared JP/US source
  is realistic.
- **EU is the same game source recompiled with a newer compiler and SDK,** plus
  PAL and multi-language changes. The structure survives (85% of JP bytes have
  a shape partner at ≥ 0.3), but almost nothing is byte-identical. It is a
  separate matching target with its own compiler profile.
- **KF2 is largely new code, not a KF1 derivative.** Only about 45 small
  helpers survive unchanged from KF1. The KF1 source tree helps with engine
  concepts, formats and SDK handling, not with direct function reuse.

## 5. What is available on archive.org

| Needed for | Item / file | Contents | Status |
| --- | --- | --- | --- |
| jp/us SDK + tools | [`psyq_psx_toolchain_april_08_1994`](https://archive.org/details/psyq_psx_toolchain_april_08_1994) `psx.zip` (48.8 MB; SHA-256 `416241637cdb0273b6bd936b0ab5627c8ba7225ee2b98066cf3b50fb09e57899`) | Psy-Q PS-X Development System Release 3.0; files dated **1995-04-08** despite the item title. `LIB/*.LIB`, `INCLUDE`, `CC1PSX` 2.4.1, `CC1PSX.EXE` 2.6.0, ASPSX 2.08, CCPSX 1.10, PSYLINK 1.29, CPE2X | **Downloaded. RCS revisions match JP/US exactly.** ASPSX 2.08 has no software-key strings (untested under DOSBox). |
| jp/us SDK (Sony CD) | `ps1_sdks` "Programmer Tool - Runtime Library Version 3.0 (Japan) DTL-S2180" (68 MB; SHA-256 `0717a820197d337e53696cbabae4a1252f5e20339686d30c5929be37faf4ef37`) | Official Sony 3.0 CD | **Pinned for its `PSXGRAPH/BIN/CPE2X.EXE`**, the converter that wrote the retail PSX/OPEN/END headers |
| eu SDK | Runtime Library **3.4** | — | **Not found on archive.org.** `ps1_sdks` has 3.3 (DTL-S2190) and 3.5 (DTL-S2300) as neighbours. The Ghidra corpus has 3.4 signatures, but those are not `.LIB` files |
| eu compiler | [`psyq-sdk`](https://archive.org/details/psyq-sdk) `PSYQ_SDK.zip`, member `psyq/psyq/CC1PSX.EXE` | GCC **2.7.2.SN.1** (1995-12-20), ASPSX 2.34, CCPSX 1.18, CC1PLPSX | Members downloaded individually |
| later tools | same zip, `psyq/bin/` | GCC 2.95.2 (1999), SDevTC ASPSX 2.86 | Too late for any KF2 build |
| linker/librarian only | `ps1_sdks` `PSY_ExecutablesFrom1995.zip` | PSYLINK (1995-12-21), PSYLIB (1995-11-08) | Possible EU-era linker witness |
| GCC 2.6.0 disk | `ps1_sdks` "GNU C Compiler Version 2.60" | GCC 2.6.0 (1994-10-20) | Already used by KF1 |

Downloaded witnesses live outside the repository
(not under version control). They are proprietary: hash them and keep them
external; don't commit them.

## 6. Next steps to prove the compiler

1. Run the 3.0 kit's `CC1PSX` 2.4.1 and ASPSX 2.08 under DOSBox, using the go32
   stub transplant from the KF1 notes. Compile small JP leaf functions and
   compare against the KF1 `cc1psx-257` rebuild. First choose functions that
   are byte-identical between JP and US.

   Partly done: the `gcc241-kit` compiler (`probe-gcc241-kit-o2-r2000`) stages
   the kit's 2.4.1 image behind the kit `CC1PSX.EXE` go32 stub and runs it
   under DOSBox with ASPSX 1.07. With every GAME unit switched to one profile
   (sources tuned for 2.5.7), exact functions were: 2.5.7 `-mcpu=r2000` 479,
   2.4.1 `-mcpu=r2000` 419, 2.4.1 default CPU 273, decompals 2.6.0 97 (its
   `addu sp; j ra; nop` epilogue fails every framed function; the kit's real
   `CC1PSX.EXE` 2.6.0 emits the same epilogue). No unit gains under 2.4.1, but
   it alone reproduces `menu_fade_transition` exactly: 2.5.7 keeps the
   `scene_display_sys+0x10024` ordering-table address as a loop-movable
   pseudo, which spends one loop-invariant motion and changes which constants
   are hoisted. 2.4.1 also reproduces the retail register choice in
   `render_textured_quad` except one folded constant. It loses 2.5.7 forms that
   retail has: `divu` for `u8` operands, frameless counted copies, and the
   2.5.7 constant-multiply sequences. Retail therefore looks like neither
   pinned build; an SN GCC between 2.4.1 and 2.6.0 remains plausible.
2. Do the same for EU with GCC 2.7.2.SN.1, sweeping `-G` (for example `-G8`)
   and `-O2`, and compare with the 2.6.0 binary.
3. Rerun the signature census against the real 3.0 `.LIB` files through psy-k
   (KF1's `fid_census.py`) to settle JP's unmatched movie-path objects. Then run
   a skew-tolerant pass over the game span.
