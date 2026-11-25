# Readability metrics

`kf verify readability` counts the source shapes that a decompiler or a
matching session leaves behind and a programmer would not have written. It
complements `kf verify board`, which counts address-derived names, externs
and casts. The scan is lexical and needs no build:

```sh
python3 -m scripts.kf.verify readability                 # totals against the floors
python3 -m scripts.kf.verify readability --gate          # exit 1 if a gated metric rose
python3 -m scripts.kf.verify readability --functions 20  # worst functions first
python3 -m scripts.kf.verify readability --list "raw case labels"   # file:line sites
python3 -m scripts.kf.verify readability --metrics       # each metric and its fix
```

Gated metrics are a down-only ratchet with floors in
`config/cleanliness/readability-baseline.tsv`; after a cleanup lowers a count,
bless the new floor with `--update`. Informational metrics are leads without a
floor. A count is not a verdict: when the match forces a shape, the site stays
and so does its share of the floor. The matching rules in `CLAUDE.md` still
come first; a rewrite that loses a banked function is not a cleanup.

Literal and control-flow metrics look only inside function bodies, past
initializers, enum bodies, `#define`s, claims and layout checks: a value in a
table or a named definition already has a home.

## Metrics

| Metric | Gated | Decompiler shape | Readable form |
| --- | :-: | --- | --- |
| offset-named members | yes | `word_12`, `unknown_05`, `unk1B4` | a name for what the member holds |
| offset-named member uses | yes | `target->word_10.value` | falls with the members above |
| offset-named types | yes | `KfTargetCandidateWord0c` | a type named after its meaning |
| value-echo constants | yes | `KF_EFFECT_KIND_102 = 102` | a name for the kind; the number stays as its value |
| numbered functions | yes | `resource_transition_set_phase_3` | a name for what differs between the variants |
| placeholder parameters | yes | `arg3`, `unknown` | a name from the uses and the call sites |
| raw case labels | yes | `case 4:` | `case KF_..._NAME:` |
| raw state values | yes | `record->phase = 2` | an enum value (`*_step` increments and the continuous phases in `CONTINUOUS_PHASE_FIELDS` are skipped) |
| raw bit masks | yes | `flags & 0x40` | a named flag (masks of the form 2^k-1 are truncations and are skipped) |
| raw hex literals | yes | `0x140`, `0x2000` | a named constant, or a derivation (`sizeof`, a shift, a field width) |
| literal element fills | yes | 4+ consecutive `a[N] = literal;` | a table or string, if the match allows |
| gotos | yes | any `goto` | structured flow |
| unlisted gotos | yes | a `goto` absent from `docs/retained-gotos.tsv` | rewrite it, or record the evidence for keeping it |
| long functions | yes | body over 150 lines | extracted helpers, if the match allows |
| deeply nested functions | yes | block depth 5 or more | early returns, extracted cases |
| union scalar views | yes | `.value`, `.bytes`, `.halves` of a word union | the member that means something |
| matching-history comments | yes | "objdiff", "98%", "fake", "m2c" | comments about behaviour only |
| addresses in comments | yes | "written by 0x8002c670" | the function or datum name |
| repeated magic values | no | one value spelled in 4+ functions | one shared named constant |
| raw shift amounts | no | `>> 12` | a named fixed-point scale |
| negative indices | no | `p[-1]` | an index from the owning object |

## Where the work is

`--functions` ranks functions by gated findings. Most of the offset-named and
union-view findings come from the `KfTargetCandidate` word unions in
`include/kf/game/actor.h`, read by `actor_update_behavior` and
`actor_score_target_candidate`. Naming those members clears several metrics at
once. Most value-echo constants belong to four enums: `KfEffectKind`,
`KfObjectId`, `KfEffectRenderId` (`KF_EFFECT_MODEL_*`) and `KfActorTargetType`. Naming one enum
value also clears the raw case labels and raw state values that spell it.

When the rules here are wrong (a false positive, or a site that should be
exempt), fix the rule in `scripts/kf/readability.py` and add a case to
`tests/test_readability.py` rather than raising a floor.
