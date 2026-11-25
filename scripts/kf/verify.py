"""kf verify - cleanliness, data matching, ownership and target relink checks.

The board/data gates adapt the sibling gruntz workflow; ownership and target
relink checks extend it for King's Field II's PS-X ELF / objdiff flow:

    kf verify board [--gate|--update|--externs]        source cleanliness ratchet
    kf verify board --data [all]                    data ownership inventory
    kf verify readability [--gate|--update|--list L|--functions N|--metrics]
                                                    decompiler-shaped source ratchet
    kf verify data  [--image I][--detail][--coverage]  strict data gate vs retail
    kf verify reachability [--image I][--output P]   known-reference ownership audit
    kf verify roundtrip [--image I][--output P]      target relink/placement gate

``board`` counts the address-derived spellings and ``extern``/cast/view
crutches in ``src/`` + ``include/`` and the curated identity TSVs against
committed floors (``down = good``). Data ownership and raw DAT-name counts are
informational and have no floors; ``data`` compares each reconstructed unit's
owned ``.data``/``.rodata``/``.bss`` against the retail-delinked target,
including implicit relocation addends, and exits nonzero on any mismatch or
missing artifact. See the module docstrings for the full contract.
``reachability`` follows the shared reference census from every game function,
including vendor calls and pointer tables. Candidate, config-only, unmodeled and
unresolved paths remain diagnostics; this is not yet exhaustive byte closure.
``roundtrip`` independently links manifested target objects at claim-derived
section bases and compares initialized bytes with verified retail. It rejects
unplaceable ownership; it does not prove reconstructed whole-image equality.
"""

from __future__ import annotations

import sys


def main(argv: list[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if not args or args[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    sub, rest = args[0], args[1:]
    if sub == "board":
        from scripts.kf.cleanliness import main as board_main

        return board_main(rest)
    if sub == "readability":
        from scripts.kf.readability import main as readability_main

        return readability_main(rest)
    if sub == "data":
        from scripts.kf.data_match import main as data_main

        return data_main(rest)
    if sub == "reachability":
        from scripts.kf.data_reachability import main as reachability_main

        return reachability_main(rest)
    if sub == "roundtrip":
        from scripts.kf.roundtrip import main as roundtrip_main

        return roundtrip_main(rest)
    print(f"kf verify: unknown subcommand {sub!r} (choose board, readability, data, reachability or roundtrip)",
          file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
