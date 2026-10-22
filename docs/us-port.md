# US port (SLUS-00158): state and worklist

This fork targets the US disc. `scripts/us_port/README.md` explains how the JP
model was retargeted. Retail files live in the ignored `disc/`: `disc/us/exe/`
is the `kf init` directory (it holds `PSX.EXE`, a copy of `SLUS_001.58`) and
`disc/jp/` holds the JP disc and executables for comparison.

## Baseline

`kf analyze` against the US executables: GAME 518/524, OPEN 15/26, END 7/13,
PSX 0/1 exact. GAME data modules match 5/16 (menu_root's .data is exact); the
rest is mostly the COMMON-bucket placement upstream also lacks
(`kf common-order`).

## US facts the model now carries

- Relocations rescanned inside changed functions are byte-validated and
  accepted (`us-port:scan;byte-validated` in `relocs.tsv`).
- Menu text is u8 glyph codes (A=0x00..Z=0x19, space 0x7f, 0xff end) in
  24-byte rows: `KfMenuGlyphRow`, `KfMenuLabelSuffix` are `u8[24]`,
  `KfCardSlotGlyphRow` is `u8[20]`. The label, item-name and window tables in
  `menu_root.c` hold the US bytes.
- Pad-button masks are 14 separate u32 `.sdata` words at 0x8006e568..0x8006e59c
  (`include/kf/game/game.h`), followed by four state words read by
  `func_8001883c`. They are separate scalars: retail reloads `lui`+`lw` for every
  access, which an array or struct does not reproduce.
- The JP `menu_run_root_controller` body is at 0x80018e80. 0x8001883c is the
  US-only `func_8001883c`, a frame-timed state machine the CD wait loops call.
- The attack masks moved out of `player_magic_id_sequence` (now 12 bytes) into
  `.sbss` `player_magic_attack_masks` at 0x8006e994.

## Worklist

GAME, all register-allocation residue with CFG, calls and referents equal:

- `func_8001883c` (98.5%): one MoveImage argument's load order.
- `game_main_loop` (99.1%): the `li s2,1` lands one instruction early.
- `menu_card_save_slot` (91.9%): slot/width/height rotate among s0, s2, s3.
- `menu_format_number` (97.0%): retail strength-reduces the digit loop's
  index away (cursor and signed compare against `out`); ours keeps the index.
- `memory_card_write_slot` (99.2%): slot/path and attempt/checksum swap.
- `event_map_object_interact` (99.3%): five locals permute s2..s6.

OPEN and END:

- US-only OPEN 0x80012638 (0x6f0) has no claim yet, so OPEN shows 25 started
  of 26.
- US OPEN plays OP0-OP2.S and loads `OPU.D`. US END plays ED0.S then ED1.S with
  no sequence audio, so `ending_play_movie(s32)` is a compile-only placeholder.
  US END's `audio_initialize` is 0x4c long, against 0x8c in OPEN.
- Constant-only differences remain in `opening_draw_prompt` (compare 97, JP 101)
  and `opening_open_audio` (VAB body 0x4e400, JP 0x497cc).

## Not yet redone for US

- `functions_vendored.tsv` for END: 13 JP SDK functions have no US copy, and
  US END and OPEN link some library members in a different order. Rerun
  `kf-fid-census`.
- Tests under `tests/` still carry JP addresses, and the curated-inventory
  test stops on retarget leftovers (e.g. `memory_card_buffer_storage`'s extent
  against the US load end).
- `scripts/kf/emulator.py` and `sector_archive_oracle.py` pin JP disc data.
