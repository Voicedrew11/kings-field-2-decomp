# US port (SLUS-00158): state and worklist

This fork targets the US disc. `scripts/us_port/README.md` explains how the JP
model was retargeted. Retail files live in the ignored `disc/`: `disc/us/exe/`
is the `kf init` directory (it holds `PSX.EXE`, a copy of `SLUS_001.58`) and
`disc/jp/` holds the JP disc and executables for comparison.

## Baseline

`kf analyze` against the US executables: GAME 524/524, OPEN 26/26, END 13/13,
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

## Shapes that closed the last three

- `memory_card_write_slot`: `write()` and `read()` results go into one
  `transferred` local inside nested `if`s, and the checksum compare puts
  `checksum` first. CSE then reuses the 0x4000 constant register for the
  `read()` compare instead of the dead `write()` result.
- `menu_card_save_slot`: one local carries the probe dialogs' overlap (2) and
  then the write dialogs' height (66), assigned in the write call's argument
  list. The two failure arms each call `menu_draw_card_dialog_rows` (jump2
  cross-jumps them) and share one wait. Forward `goto done` replaces
  `do { } while (0)`.
- `func_80012638`: literal UV corners (hoisted per loop), x written as
  `0xc0 + i * 2` (strength-reduced with x+255), CLUT as `row * 64 + 1`, `i`
  reused for the second quad's level, and no `mode < 13 - phase` guard: that
  test is the `for` loop's entry check with `mode` (known 0) standing in for
  `i`. The second quad is drawn once the frame counter reaches 88.

## Not yet redone for US

- `functions_vendored.tsv` for END: 13 JP SDK functions have no US copy, and
  US END and OPEN link some library members in a different order. Rerun
  `kf-fid-census` over the remaining coverage gaps (only the two serial-audio
  members have been admitted so far).
- Tests under `tests/` still carry JP addresses, and the curated-inventory
  test stops on retarget leftovers (e.g. `memory_card_buffer_storage`'s extent
  against the US load end).
- `scripts/kf/emulator.py` and `sector_archive_oracle.py` pin JP disc data.
