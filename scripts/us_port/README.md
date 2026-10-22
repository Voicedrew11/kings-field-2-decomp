# JP -> US retarget

This fork targets the US disc, `SLUS-00158`. Upstream's model (functions, data,
relocations, source claims) was built for the Japanese `SLPS-00069` at upstream
commit `7a8e5f0`. These scripts moved it to US addresses.

The map builders take a work directory `W` holding `jp/` and `us/` (the four
executables extracted from each disc) and `jpcfg/` (the JP `config/retail`
tables from `7a8e5f0`), and run in this order:

| script | writes | what it does |
| --- | --- | --- |
| `bytediff2.py W .` | `W/jp_us_map.tsv` | finds each JP function in US, masking jump targets, `lui` and register-relative immediates |
| `runs.py W` | (report) | splits each run of changed functions at `jr ra` in the US gap |
| `fmap_final.py W` | `W/fmap_final.tsv` | final function map, with the hand-checked overrides in the script |
| `datamap.py W .` | `W/datapairs.pkl` | JP -> US data targets read from HI16/LO16 pairs at the US sites |
| `addrmap.py W .` | `W/addrmap.tsv` | places every curated data item: reloc anchors, then byte comparison |

Their outputs are committed as `config/us_port/function_map.tsv` and
`config/us_port/address_map.tsv`; `config/us_port/data_overrides.tsv` holds the
data placed by hand. Then:

```sh
python3 scripts/us_port/retarget.py --us-dir disc/us/exe   # rewrites config/ and src/ claims
python3 scripts/us_port/hand_edits.py                      # US END/OPEN structure, US-only placeholders
```

`retarget.py` prints what it could not translate. Functions the US build
changed lose their baseline rows and get relocations rescanned from US code.
