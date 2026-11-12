# Pointer zeros, Booleans and the modern check

This is the lane E2 cleanup that brings KF1's NULL, Boolean and
compiler-warning passes to KF2. Every change keeps all four executables and
every assembler output byte-identical, apart from debug line records. The
[enum-domain plan](enum-domain-plan.md) owns the non-Boolean literals.

## Pointer zeros

```sh
nix develop -c python -m scripts.kf.pointer_zeros            # both language views
nix develop -c python -m scripts.kf.pointer_zeros --mode retail
```

The census parses all 39 C variants in the retail C view and the modern C++
view. It reports literal zeros converted to a pointer. Zeros expanded from
`NULL` are not reported. Before this pass it found 407 sites. Almost all were
layout checks that used `(u32)&((T *)0)->member`. Psy-Q 3.0 `STDDEF.H` has
no `offsetof`. `kf/lib/offsetof.h` supplies it: the pinned GCC keeps the
null-object member address, spelled with `NULL`, and C++ uses
`__builtin_offsetof`. Two movie callback resets also became `NULL` once their
SDK prototypes were declared. The census now reports 0 sites in both views.

In the C++ view, `NULL` is `nullptr`. The retail C view keeps Psy-Q's `0`.
Clang's `-Wzero-as-null-pointer-constant` drops from 298 project diagnostics
to 14. All 14 come from Psy-Q `LIBCD.H` macros (`CdSeekP`, `CdSeekL`,
`CdPause`, `CdStop`), so the warnings stay.

`kf literals` reports 333 pointer-zero sink rows. Of these:

| Rows | Verdict |
| ---: | --- |
| 282 | Already spelled `NULL` |
| 6 | Non-zero fixed addresses: the overlay mailbox and heap bases. These need literal-meaning review; they are not null pointers. |
| 32 | Integer variadic arguments of `effect_construct_record` (indices 0/1/2 and parameters). The census reads them as pointer arguments. |
| 13 | `memset`/`memcpy` fill bytes and sizes passed through the K&R `MEMORY.H` declarations |

## Booleans

`kf bools` drops from 91 candidates to 17. The Boolean types keep the retail
storage width and signedness: `b8`/`b16`/`b32`, `KfBool` (`int`),
`KfBoolU32`, and the new `KfBoolS16`. `KfBoolS16` is needed because
`KfResourceState.transition_active` is loaded with `lh`. Comparisons keep
their retail shape:

- `== 1` becomes `== KF_TRUE`, because a test against 1 differs from a truth
  test.
- `!= 0` and `== 0` become truth tests, which compile the same.

`player_move_horizontal` used a chained zero assignment that mixed a counter
with three flags. It is now four plain assignments, and the output is
unchanged.

Rejected, following KF1's rules:

- Counters, indices, quantities and masked flag bits.
- Values loaded from resources or the card payload, whose shipped values were
  not checked: `KfMagicRecord.menu_available`, `KfAnimKeyframe.reverse`,
  `KfAssetHeader.animation_present` and resource-record bytes.
- Bytes in unions whose other views write the same byte:
  `KfEffectCollisionLatch.impact_handled` and the map-object tail fields.
- Existing enum results (`KfAudioPlaybackResult`).
- `menu_load_item_model`. It is a failure predicate, but in this build every
  path returns 0.

Handed to the enum lanes, because the 0/1 values belong to a named or larger
domain:

- The six player option bytes, the card snapshot copies, the options menu's
  `selected` rows and `menu_draw_options_rows`. KF1 models these as
  `KfPlayerOption`, and they feed `render_model_rows[].state`.
- `menu_format_number` padding mode (KF1 `KfFormatPaddingMode`).
- The OPEN title, banner and prompt phases, and `main::state`.
- `main_end_flag`; `KfCdRequest.stream_complete` and `phase`.
- The trajectory mode (`actor_start_ballistic_motion`,
  `trajectory_solve_time_angle`).
- The drop source of `map_object_spawn_effect`.
- `map_object_set_cell_marker`/`refresh_cell_markers` mode
  (`KfMapObjectProperty`).
- `player_update_weapon_attack::mode` and `weapon_attack_mode`
  (`KfAnimationClip`).
- The switch-state selectors (`find_map_cell_layer_mask_run_boundary::state`,
  `KfEffectKind46State.phase`, the cell-copy `transition_mode`).

`kf literals` reports 406 boolean-candidate domains (856 literal sites).
[boolean-domain-review.tsv](boolean-domain-review.tsv) gives every domain a
verdict: 69 converted, 318 rejected with a reason, and 19 handed to the enum
lanes.

## Modern check

`kf check-types` passes 39/39 variants; it passed 18/39 before this pass. The
fixes:

- Declare the SDK entry points that Psy-Q 3.0 headers omit or comment out:
  `CdInit`, `CdDataCallback`, `DecDCToutCallback`.
- Add a `CONVERT.H` wrapper with a C++ `atoi` prototype.
- Bind BIOS `delete` by its linked name in C++.
- Include `LIBSPU.H` and `pad.h` where their functions are called.
- Spell explicit void casts at SDK/libc boundaries, as on SLPS-00017. The
  `void*` ratchet floor records them.
- `resource_request_transition` returns no value on any path, but retail
  schedules its epilogue for a live `v0`. Declaring it `void` moves `li v0,1`
  into a delay slot and removes a load-delay `nop` in a caller. It keeps
  `s32` in C; `KF_VALUELESS_S32` checks it as `void` in C++.
- `menu_root` keeps a single forward static, for `current_poly_ft4`; its
  claimed repetition is hidden from C++. Moving the data block above its
  consumers instead makes two struct copies word-aligned and changes code.

## Compiler-warning triage

`python3 -m scripts.kf.warnings --output-dir build/warning-review/...`

Safe fixes:

- Character literal for the card path terminators.
- `sizeof` sizes for the save-payload copies.
- `KF_COUNTOF` (signed) loop bounds.
- One unreferenced label removed.
- Locals that shadowed BIOS/libm names or another local renamed.
- `const` overlay path table.

Kept, with evidence:

- The documented stack-slot locals (`frame_reserve`, `rect`, `start`,
  `clip_vertices`, `unused`).
- The block-statement subdivide and menu-fade macros. Rewriting them as
  `do { } while (0)` changes register allocation in
  `tmd_prepare_subdivided_object` and the menu fade.

KF1 has no literal-suffix convention: 679 of its 683 address-sized constants
carry no `U`. The GCC "unsigned in ANSI C" diagnostics are therefore left to
the literal-meaning review.
