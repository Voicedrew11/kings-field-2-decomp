# US port (SLUS-00158): state and worklist

This fork targets the US disc. `scripts/us_port/README.md` explains how the JP
model was retargeted. Retail files live in the ignored `disc/`: `disc/us/exe/`
is the `kf init` directory (it holds `PSX.EXE`, a copy of `SLUS_001.58`) and
`disc/jp/` holds the JP disc and executables for comparison.

## Baseline

`kf analyze` against the US executables: GAME 454/524, OPEN 15/26, END 7/13,
PSX 0/1 exact. Every function the map calls unchanged is exact. The data
modules are below JP's count (GAME 4/16, OPEN 3/8, END 1/5), partly because
US data changed, partly the COMMON-bucket placement upstream also lacks
(`kf common-order`).

## Worklist

Every function below is unbanked. `config/us_port/function_map.tsv` gives
each one's JP and US address and size. Start from the JP C that already
exists: most differ from JP by a few instructions.

- **Constants only** (same shape, immediate differs): `opening_draw_prompt`
  (compare 97, JP 101) and `opening_open_audio` (VAB body 0x4e400, JP
  0x497cc). There are more among the GAME shop and menu controllers and
  `render_*_overlay`. They are probably English layout coordinates.
- **Small edits** (72-99% similar): `player_update_frame`,
  `player_update_weapon_attack`, camera, motion and charge functions,
  `effect_update_dispatch`, `event_map_object_interact`,
  `notification_update`, `game_main_loop`, `cd_read_sectors`,
  `cd_request_wait_idle`, `memory_card_write_slot`, boot `main`.
- **Rewritten:** the memory-card menus (`menu_prompt_two_option` ..
  `menu_prepare_card_exit_rows`), the status and shop screens
  (`menu_buy_owned_items` .. `menu_draw_combat_attributes`),
  `menu_run_root_controller`, and `menu_draw_string`/`menu_draw_number`
  (English text).
- **US-only, not reconstructed:** GAME `func_80018e80` (0x2ac) and
  `func_800196c8` (0x10c) are empty placeholders in `src/game/menu_root.c`;
  OPEN 0x80012638 (0x6f0) has no claim yet, so OPEN shows 25 started of 26.
- **OPEN/END:** US OPEN plays OP0-OP2.S and loads `OPU.D`. US END plays ED0.S
  then ED1.S with no sequence audio, so `ending_play_movie(s32)` is a
  compile-only placeholder. US END's `audio_initialize` is 0x4c long, against
  0x8c in OPEN.
- **Data:** the menu label tables are 0x18 per row in US (JP 0x14).
  `player_magic_id_sequence` loses its 8 bytes of attack masks, and
  `main_unreferenced_sdata_bytes` is provisional. See
  `config/us_port/data_overrides.tsv`.

## Not yet redone for US

- `functions_vendored.tsv` for END: 13 JP SDK functions have no US copy, and
  US END and OPEN link some library members in a different order. Rerun
  `kf-fid-census`.
- Tests under `tests/` still carry JP addresses.
- `scripts/kf/emulator.py` and `sector_archive_oracle.py` pin JP disc data.
