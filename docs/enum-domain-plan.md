# Enum domain plan

This plan turns KF2's bare integer literals into enum domains the way the KF1
cleanup did (KF1 `docs/patterns/enum-equality-review.md`,
`enum-reuse-review.md` and `strict-enum-audit.md`): one domain per meaning,
joined by retail value flow rather than by equal spelling, with
alias/linked/retain verdicts for equal values. The curated rows
are in [enum-domain-plan.tsv](enum-domain-plan.tsv). Domain IDs such as `D0010`
are census output and change between runs; the TSV is keyed by the proposed
enum name and its member slots.

## Census

`kf literals` (`scripts/kf/literals.py`) parses all 39 manifest C variants with
the retail Clang flags. Every written literal, enum constant and object-like
macro constant is recorded at its spelling site with the sink that consumes
it: `Type.field`, `fn::local`, `fn:argN`, `fn:return`, switch subject, array
index, mask, shift, arithmetic operand, declaration size or pointer zero.
Masked and shifted reads (`x&0xf`, `x>>4`) are separate packed sub-slots.

Domains are unions of sinks connected by retail value flow: copies,
initializers, call arguments, returns, equality between two slots, slots
indexing the same array, and a switch selector whose case values are stored
unchanged in another slot for two or more cases. Slots written by arithmetic
or ordered against another variable are quantities: they keep their literals
but do not join domains. Slots with more than 12 flow partners, or function
locals with more than 4, are reported as hubs and also do not join. A domain is
a lead; the verdicts below are the review.

```sh
nix develop -c kf literals --output build/literals.json   # full JSON
nix develop -c kf literals --domains                       # one row per domain
nix develop -c kf literals --member KfMapObject.action     # domains of one slot
nix develop -c kf literals --value 0x58                    # domains using a value
```

Each domain lists members with declared type and width, literal values,
enum/macro constants already used (with header), the prefix family of those
constants, joining edges with file:line, and up to five KF1 counterparts. KF1
is read from `$KF1_REPO` or a sibling `kings-field` checkout: a qualified
`Record.field` or `fn:argN` typed by a KF1 scoped enum scores 4, a used
constant 2, a bare field name 1, with value overlap as tie-break.

Numbers at `b137710` (after the two applied domains):

| Measure | Count |
| --- | ---: |
| Written numeric tokens in `src/` and `include/` | 20,230 |
| ... parsed by at least one variant | 18,716 |
| ... retail claims (`ADDRESS`, `DATA`, `RODATA`) | 1,464 |
| ... `sizeof` operands / unattributed macro bodies / other | 24 / 22 / 4 |
| Literal locations (all files / `src/`) | 18,659 / 15,837 |
| Literal sink rows (a location can feed several variants' sinks) | 18,972 |
| Enum-constant / macro-constant sink rows | 3,117 / 740 |
| Integral slots / flow edges | 4,977 / 3,881 |
| Candidate domains / multi-member / spanning several owners | 1,454 / 196 / 165 |
| Domains joining two or more existing constant families | 111 |
| Hub slots | 109 |

Literal sink rows by class:

| Class | Rows | Treatment |
| --- | ---: | --- |
| `init` | 5,437 | data tables; field domains only where a flow joins them |
| `declaration` | 2,623 | array extents, bit widths, layout checks: numbers |
| `argument` | 2,153 | parameter domains |
| `assign` | 1,784 | field/variable domains |
| `index` | 1,706 | array positions; a domain only for enumerated axes |
| `compare` | 1,350 | field/variable domains and range bounds |
| `arithmetic` | 1,053 | scale, offset and step quantities |
| `enum-definition` | 801 | already named |
| `geometry` | 590 | SDK vector/rect/colour/primitive fields: numbers |
| `case` | 516 | switch-subject domains |
| `shift` | 425 | shift counts: numbers or format constants |
| `mask` | 215 | flag and packed-field domains |
| untracked (`compare`/`init`/`assign`/`argument`) | 180 | computed operands, unnamed aggregates |
| `return` | 70 | return domains |
| `pointer-zero` / `truth` / `typed-boolean` | 54 / 5 / 10 | lane E2 |

Domain verdicts (literal sites in parentheses): enum-candidate 217 (2,711),
flags-candidate 49 (203), review 56 (192), boolean-candidate 408 (1,090),
singleton 110 (205), quantity 440 (1,888), data-table 71 (5,359),
array-index 99 (1,543), typed-enum 4 (0). Heuristic verdicts are a triage
order, not decisions; the curated TSV records decisions.

## Applied domains

Both changes keep all 564 functions exact. Every PSX/GAME/OPEN/END EXE and all
assembler code lines are byte-identical to the pre-change build; only debug
records for the new enum constants and the moved source line change. The
modern check keeps the same 177 diagnostics in the same 21 variants (all
pre-existing pointer-policy findings); a raw `object->action == 0xff` in the
modern view now fails with an enum/int operand error. `kf check-types` also
runs the census and fails on any written literal whose sink is an enum-typed
field, parameter, return, promoted local or switch subject (a cast would
otherwise hide it); there are none.

**`KfMapObjectOperation` (u8, `include/kf/game/map_object.h`).** Template byte
0 (`KfMapObjectTemplate.collision_kind`) and the runtime action byte
(`KfMapObject.action`) are one namespace. `map_object_initialize_from_placements`
loads template +0 at GAME `0x80035a74`, dispatches through its jump table and
stores the same immediate at object +4 in each case (`0x80035c78 li 0x53;
sb +2(s0)` with `s0` = object + 2). `map_object_start_action_if_idle`
(`0x800363bc`) stores its argument unchanged; `event_world_dispatch_interaction`
and `event_world_state_save_slot` switch on the template byte, while the update,
render and save switches read the action. The enum types both fields, the
setter parameter and the two switch locals (`KF_ENUM_PROMOTED`). It replaces
136 literal sites and the separate `KF_MAP_OBJECT_ACTION_*`,
`KF_MAP_OBJECT_MOTION_ACTION` and `KF_MAP_OBJECT_COLLISION_KIND_HINGE`
constants. Names come from those constants (whose 96-98 encodings and names
match KF1's `KfMapObjectOperation`) and the `resource_trigger` tail view; 35
members keep WIP decimal names, matching the existing `action83`/`action224`
view names.

**Placeholder operations against retail data.** A one-off read of the JP
disc decoded the templates (FDAT 48 chunk 0), each object's model (MO `0x80 + id`), the
placements of map regions 0-7 and 10 (FDAT `3r + 1`, object bytes `0x38..0x3f`
from placement bytes 16-23) and the operation immediates of map callback slots
8 and 9 (FDAT `3r + 2`). Every model below is a TMD whose primitives are all
textured. "Collision" means a nonzero template radius: only then does the
loader add cell occupancy and `map_object_find_collision_at_point` (the
`KF_COLLISION_QUERY_MAP_OBJECTS` part of `collision_query_world`) report the
object. No placement uses layer mask 0, every one is drawn by the ordinary
render path, and none of these operations is in KF1's numbering with the same
behaviour (KF1 0/11/80/81 are a hinged door, restore point, projectile emitter
and swing release).

| Op | Objects | Collision | Model | Placements | Behaviour (GAME, then map callbacks) | Name |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | none | - | - | no template | save state byte (same save arm as 80); init falls to callback 8, which no map handles | WIP `OP_0` |
| 11 | none | - | - | no template | init hides it (layer 0) like hidden screen image 20, no interaction | WIP `OP_11` |
| 15 | 189 | radius 400 | 443 FT3 | 19, regions 0-6 | items 114..116 used on it are spawned in the event pool and lowered onto it; items 111..113 (10 MP) warp back in front of it; empty socket notifies 16; markers refused; pending item saved | `RECALL_SOCKET` |
| 17 | 184 | none | 88 FT3 + 16 GT3, unbiased depth | 28, pairs in regions 1/4/5/6 | item 103 used on it is spawned and slid in, then ORs bit 1 or 2 into linked signal door 139's `0xfc` marker (open from front/back); markers refused | `DOOR_SOCKET` |
| 33 | 239 | radius 600 | 20 FT3 | none | loader arm shared with 255 (no init, skips callback 8); no update or save case, and unlike 255 no interaction notification | WIP `OP_33` |
| 48 | 246 | none | 2 GT3 (one quad) | 1, region 1 layer 1 | additive blend, raised by byte `0x3c` * 256 (`0x14`); no update case in any map | `RAISED_ADDITIVE` |
| 80 | none | - | - | no template | marker-signal receiver, saved; no update in any map | WIP `OP_80` |
| 81 | 200, 201, 202, 213 | none | 27 / 222 / 118 / 118 textured | 8 / 16 / 13 / 5 | plays its clips when the camera region (200/201: always) or an interaction (202/213: region never) starts it, probes actors and player at a model vertex and applies hazard damage | `ANIMATED_HAZARD` |
| 95 | 204 | none (rewrites cells) | 102 FT3, 22 semi | 1, region 0 | marker receiver; region 0: loops sound `0xed`; marker `0xf0` plays the clip, copies a 3x2 cell block at half phase and fades the sound out | `SIGNAL_CELL_COPY` |
| 160 | 185 | none | 132 FT3 | 1, region 0 | region 0: item 101 arms it; it moves placed item 101 into itself, then signals `0xf0` (opens 95) | `REGION0_ITEM_SOCKET` |
| 161 | 187 | radius 1024 | 104 FT3 | 3, region 1 | region 1: item 105 arms it; it lowers placed item 105 into itself, then signals its byte `0x3a` (`0xc5`/`0xc6` start 162) | `REGION1_ITEM_SOCKET` |
| 162 | 196-198 | none | 217 FT3, 52 semi | 1 each, region 1 | marker receiver, not saved; region 1: plays its clip, sets event bit 1/2/4 that enables a floor restore spot, loops sound `0xf0` | `SIGNAL_EVENT_BIT` |
| 163 | 192 | radius 800 | 466 GT3, 124 semi | regions 1 and 3 | marker receiver, saved; region 1 activates when bits 1/2/4 are all set, sets bit 8 and signals `0xc8`; region 3 shows it active once bit 8 is set; loops restore sound `0xee` | `EVENT_BIT_ACTIVATED` |
| 164 | 188 | none | 670 FT3 | 2, region 4 | saved; region 4: item 109 is inserted, each interaction steps byte `0x38` through 12 positions and the item turns to `n * 4096 / 12` | `ITEM_DIAL` |
| 165 | 276 | radius 1500 | 312 textured, 192 semi | 8, region 6 | interacts like `NONE`; region 6: lighting row `0x3f`, and while drawn and slot 192 (item 94) remains it spawns effect kind 101 around itself with random sound `0xf5` | `EFFECT_EMITTER` |

Region 10's callback table points outside the `0x8019e138` workspace and none
of its placements use these operations.

**Operations 0, 11, 33 and 80 have no producer.** An exhaustive one-off check
of the JP, US and EU discs found no template with operation 0, 11 or 80, and
the single operation-33 template (object 239, an identical row on all three
discs) is never placed in regions 0-7 or 10, never a death drop (actor byte 6
or target-candidate byte `0x0c`) and never spawned by item use, events or map
callbacks. No code writes any of the four into the action byte: GAME stores
only named immediates (`event_world_state_restore_slot` writes 96/97/98/112,
`map_object_start_action_if_idle`'s five callers pass 0x60-0x62, and the
startup clear's 0 is overwritten with 0xff by `map_object_pool_reset`); the
whole of every region's FDAT `3r + 2` code (all 32 slots; region 10 at its own
link base `0x80193b38`) initializes only 0x20, 0x5f and 0xa0-0xa5 and never
calls the setter; OPEN and END have no map-object code. With every writer an
immediate, no script, event or talk byte can select one of them. GAME keeps
only dispatch hooks: 0 and 80 share a save arm (`0x800488c4`), 80 joins the
marker-signal receivers (`0x80036744`), 11 shares the loader arm that hides
operation 20, and 33 the inert arm of 255; US and EU GAME have the same arms.
KF1 numbers 0 hinged door, 11 restore point and 80 projectile emitter (its link
trigger also clears the link byte of 80) and has no 33. KF3 retail has no
template for any of the four; the KF3 pilot build has a hinge-door row with
operation 0 (object 178) that its loader initializes like KF2's `HINGE`.
That lineage does not show KF2 behaviour, so all four keep their WIP names. `KF_ACTOR_FLAG_DIE_WITH_LINKED`
(`0x200`) is tested once (`0x8003f4d4`, `actor_update_behavior`): when a
linked companion's partner leaves the active lifecycle the companion detaches,
blocks player targeting and, with the flag, selects target type 3 (the type
`actor_apply_damage` selects at zero health, which spawns the death drop).
No code or map callback sets or clears the bit; it comes only from
`initial_actor_flags` `0x212`/`0x612`, i.e. linked groups of definitions 5, 7
and 30 whose partners use definitions 4, 6 and 29 (regions 0-3, 52 actors).
It gates behaviour only, not rendering, collision or damage.

**`KfMemoryBlockKind` (u8, `include/kf/game/memory.h`).** The arena block header
byte, `memory_block_set_kind`/`memory_block_kind` and the two
`event_arena_owner_pointers_*` walks. Their `kind < 4` bound is the named
`KF_MEMORY_BLOCK_KIND_COUNT`; the resource registry's unsigned
`(kind - 1) < 2` range test encodes the kind explicitly. 14 literal sites.
KF1's allocator has no block-kind counterpart.

### Where `case 0x58` belongs

The value appears in two domains and stays separate:

- `map_object.c` (template selector and action switches), `map_object_action_update.c`
  `case 88` and `event_world_state_save_slot` `case 88` are
  `KF_MAP_OBJECT_OP_CELL_COPY_TOGGLE` (applied).
- `event_scene_command_dispatch(..., command)` `case 0x58` is an object/item ID:
  the same switch decrements inventory counter `0x54` for command `0x54`, passes
  key-item commands `0x63..0x6d` to `map_object_check_and_consume_marker`, and
  `0x58` signals the marker of nearby object `0x9d`. It is `KF_ITEM_MARKER_SIGNAL` in the
  applied `KfObjectId` domain. No flow joins the two; equal value is not evidence.

## Proposed domains and lanes

The TSV gives each proposed enum its storage, owning header, lane, census
members, values, existing constants, KF1 counterpart and evidence. Verdicts:

- `applied` - done on this branch.
- `apply` - one domain; type all members and replace every literal.
- `merge` - census joins several existing constant families or slots; replace
  them with one enum (KF1 `reuse`).
- `split` - the census union contains distinct domains joined by a reused
  variable or coincidental compare; type them separately.
- `linked` - retail transports one value into two meanings by construction;
  record the relationship, keep both names or an explicit boundary.
- `retain` - quantities, SDK arguments and data encodings: numbers or named
  policy constants, not enums. Rows named `(retain: ...)` group census
  domains by reason and list each domain's root slot.
- `named` - sentinels, packed fields or mixed words whose values are now
  named constants while the slot keeps integer storage.
- `e2` - Boolean or pointer zero; handled by lane E2.

Coverage rule: every `enum-candidate`, `flags-candidate` and `review` domain
in `kf literals` has a member slot named by some row here (members or
evidence), or has no literal site left.

### Lane partition

Lanes own disjoint header families. An enum lives in its owning lane's header.
When a member slot is declared in the other lane's header, the owner types its
own slots and spells the boundary explicitly (`KF_ENUM_ENCODE`/`KF_ENUM_DECODE`
or a promoted local); the other lane later replaces the boundary with typed
storage. Source files are shared; keep edits to the lane's own domains.

| Lane | Headers | Domains |
| --- | --- | --- |
| A: actors, effects, magic, items, combat, animation | `game/actor.h`, `game/effect.h`, `game/player.h`, `game/card_payload.h`, `game/collision_cache.h`, `game/animation.h`, `game/pool.h`, new `game/item.h` | `KfObjectId`, `KfEffectKind`, `KfEffectRenderId`, `KfEffectType`, `KfEffectRenderFlags`, `KfEffectPhase`, `KfEffectCollisionResult`, `KfActorSlotState`, `KfActorTargetType`, `KfActorTargetActionState`, `KfActorMotionMode`, `KfActorMotionResult`, `KfCollisionHitFlags`, `KfAnimationClip`, pad buttons |
| B: map, menu, render, audio, card I/O, events, resources | `game/map_object.h`, `game/map_cell.h`, `game/map_cell_pattern.h`, `game/map_placed.h`, `game/menu.h`, `game/notify.h`, `game/notification_quad.h`, `game/graphics.h`, `game/render_model.h`, `game/render_mask.h`, `game/tmd*.h`, `game/audio.h`, `game/card.h`, `game/event_*.h`, `game/message_stream.h`, `game/resources.h`, `game/asset.h`, `game/cd.h`, `game/memory.h`, `game/game.h`, `game/callback.h`, `lib/*`, `open/*`, `end/*` | `KfRenderQueueMode`, `KfLightingIndex`, `KfMapLayerMask`, `KfDisplayBuffer`, `KfQuarterTurn`, `KfMenuWindowKind`, `KfMenuFormatStyle`, `KfMenuChoice`, menu list/preview kinds, `KfNotificationId`, `KfNotificationQuadKind`, `KfResourceRequest`, `KfColorOverlayControl`, `KfMapObjectKind`, `KfMapObjectProperty`, transition modes, sound IDs |

Order inside each lane: large merges with KF1 precedent first (`KfObjectId`,
`KfEffectKind`; `KfRenderQueueMode`, `KfLightingIndex`), then single-header
domains. Lane A owns the IDs that lane B's map/menu code consumes; lane B
adopts them in `map_object.h`, `menu.h` and the card dialogs after lane A lands.

### Equal values kept separate

| Value | Members | Verdict |
| --- | --- | --- |
| 0x58 / 88 | `KF_MAP_OBJECT_OP_CELL_COPY_TOGGLE`; item/command `0x58` in `event_scene_command_dispatch` | retain: no flow; different switch subjects |
| 0xff | `KF_MAP_OBJECT_OP_NONE`, `KF_MAP_OBJECT_ID_NONE`, `KF_MAP_OBJECT_RENDER_TEXTURED`, `KF_MAP_OBJECT_LIGHTING_OVERRIDE_NONE`, `KF_MEMORY_BLOCK_END` | retain: idle action, free object, textured render, no lighting override and list end are different fields |
| 4 | `KF_MAP_OBJECT_OP_HINGE`; `KF_MAP_OBJECT_INTERACTION_ANY_ANGLE` | retain: an operation versus a `collision_flags` bit; they shared one anonymous enum before |
| 0 | `KF_MAP_OBJECT_OP_SAVED_INERT`; `KF_MAP_OBJECT_ACTION_TIMER_INIT` | retain: operation versus action-timer phase |
| 0x10..0x20 | `KfMapObjectOperation`; `KfMapObjectTemplate.kind` | retain: template bytes 0 and 1; byte 1 never flows into the action |
| 3 | actor slot `KF_ACTOR_SLOT_HOMEBOUND`; target type 3 | linked: `actor_update_behavior` compares `other->target_type` with the homebound `slot_state` local (`actor_runtime.c` near `KF_ACTOR_TARGET_ACTION_RETARGET_BLOCKED`). Retail reuses the register; keep the source reuse until a typed form is byte-identical |
| 0/1 | `map_object_set_property` property; `map_object_set_cell_marker` mode | linked: `map_object_refresh_cell_markers(mode)` passes one value to both (0 hides and places the marker, clearing layer and state; 1 restores the layer mask and clears the marker) |
| 1/2/3 | memory kinds 1..3; `KF_PATTERN_SELECT_*`; property codes | retain: no flow |

The `kf enums --duplicates` census has 714 declarations in 83 duplicate-value
groups. Each lane reviews the groups it touches with the KF1 alias/linked/retain
rules; only flow or shared consumers justify a merge.

### Literals that stay numbers

- Quantities (440 domains): collision heights and radii, fog distances,
  colour levels, effect speeds, scales, lifetimes and motion vararg parameters.
  Name only shared policies, as KF1 did for camera and fog limits.
- Geometry and SDK primitives: `SVECTOR`/`VECTOR`/`RECT`/`CVECTOR`/`POLY_*`
  fields, `GetTPage` and `SetBackColor` arguments, `P_TAG.len`. Use SDK macros
  where the Psy-Q headers define them; no project enums at SDK boundaries.
- Shifts, Q12 fixed point (`KF_FIXED12_ONE`), angle masks and array extents:
  existing math constants or numbers.
- Data tables (71 domains): glyph code rows, palettes and item code tables are
  encoded resource data. Initializers stay numeric unless a typed field domain
  is proven, as for `KfMapCellPatternVariant` object/shape IDs.
- Array positions (99 domains): `MATRIX.m[i][j]` and vector indices stay
  numeric; enumerated axes such as combat components become enums.

## Apply checklist

For each domain: confirm membership from the census edges and retail
loads/stores/compares; choose storage from the narrowest declared member
(`KF_ENUM_STORAGE` for wider fields, `KF_ENUM_PARAM` for promoted parameters,
`KF_ENUM_PROMOTED` for `int` locals); replace every member literal; update
`structure_fields.tsv` and `function_identities.tsv` types; rebuild and compare
EXEs and non-debug assembler lines; run `kf check-types` and compare diagnostic
sets; `kf analyze`, `kf verify board`, Ruff, pytest and `git diff --check`;
commit, then `kf bank --unit` the affected units on a clean tree.
