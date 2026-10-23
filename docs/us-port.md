# US port (SLUS-00158): state and worklist

This fork targets the US disc. `scripts/us_port/README.md` explains how the JP
model was retargeted. Retail files live in the ignored `disc/`: `disc/us/exe/`
is the `kf init` directory (it holds `PSX.EXE`, a copy of `SLUS_001.58`) and
`disc/jp/` holds the JP disc and executables for comparison.

## Baseline

`kf analyze` against the US executables: GAME 522/524, OPEN 25/26, END 13/13,
PSX 1/1 exact. GAME data modules match 5/16 (menu_root's .data is exact); the
rest is mostly the COMMON-bucket placement upstream also lacks
(`kf common-order`). PSX, OPEN and END relink; GAME does not, because it
references extern-only `.bss` and PSX-image symbols (`DAT_800864dc`,
`player_magic_attack_masks`, `DAT_800102f0`, `DAT_800102f8`) that no GAME
unit defines.

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

## More US facts

- US OPEN and END link LIBSND `SsSetSerialAttr` and `SsSetSerialVol` (FID
  HIGH); `audio_initialize` turns serial (CD) audio on at volume 0x7f.
- The movie players set a per-movie `movie_last_frame` (.sbss) that `strNext`
  compares against; US `strKickCD` reads with mode 0x1c0 (no `CdlModeSF`).
  END's `strNext` does not store `StrFrame`: `ending_play_movie` counts it.
- US OPEN plays OP0-OP2.S through `s32 opening_play_movie(s32 movie)`, skips
  them when the PSX flag byte 0x800102f8 is 1, and loads `OPU.D` (`OP.D` with
  all four right-hand buttons held).
- `func_80012638` (US OPEN) is a sweep animation over three `.sdata` words
  (0x8003e718..0x8003e720), so `open.audio` uses the G8 profile.

## Worklist

All three remaining functions agree with retail on CFG, calls and referents:

- `menu_card_save_slot` (91.9%): `slot` and the constants 0xb2, 0x42 and 2
  rotate among s0-s3; retail rematerializes 2 on the write path.
- `memory_card_write_slot` (99.2%): retail materializes 0x4000 into s3 after
  `write()` and reuses it for the `read()` compare; ours keeps `block_bytes`
  in s7 from entry. Literal compares move the constant correctly but swap
  slot and path between s2 and s3.
- `func_80012638` (92.9%): retail keeps five loop values in s0-s7 (frame 56)
  and advances x and x+255 as separate induction variables; ours saves s0-s6
  and recomputes.

## Not yet redone for US

- `functions_vendored.tsv` for END: 13 JP SDK functions have no US copy, and
  US END and OPEN link some library members in a different order. Rerun
  `kf-fid-census` over the remaining coverage gaps (only the two serial-audio
  members have been admitted so far).
- Tests under `tests/` still carry JP addresses, and the curated-inventory
  test stops on retarget leftovers (e.g. `memory_card_buffer_storage`'s extent
  against the US load end).
- `scripts/kf/emulator.py` and `sector_archive_oracle.py` pin JP disc data.
