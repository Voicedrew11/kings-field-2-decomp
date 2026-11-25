"""kf.readability - how much of the source still reads like decompiler output.

Each metric counts one source shape that a decompiler or a matching session
leaves behind and a programmer would not have written: names derived from
offsets or values, unnamed literals, union views of a scalar, gotos nobody has
justified, and comments about the matching process. Every finding carries a
``file:line``, so the same scan is a scoreboard, a lint and a work queue.

    kf verify readability                  totals, deltas against the floors
    kf verify readability --gate           exit 1 when a gated metric rose
    kf verify readability --update         MANUAL bless: rewrite the floors
    kf verify readability --list LABEL     every site of one metric
    kf verify readability --functions [N]  the N functions with most findings
    kf verify readability --metrics        each metric and how to fix it

The scan is lexical (comments and strings blanked, regex matching), like
``kf verify board``. It needs no build and no Nix shell. It is a lead, not a
verdict: a site the match forces stays, and its count stays in the floor.
Gated metrics follow the board's ratchet: they may never rise above the floor
in ``config/cleanliness/readability-baseline.tsv``, and only ``--update``
lowers a floor. Informational metrics have no floor.

Executable code excludes brace initializers, enum bodies, preprocessor lines,
retail claims and layout-check typedefs: a literal in a data table or in a
``#define`` already has a home.
"""

from __future__ import annotations

import argparse
import bisect
import re
import sys
from collections import Counter, defaultdict
from collections.abc import Callable, Iterable, Iterator, Sequence
from dataclasses import dataclass
from pathlib import Path

from scripts.kf.paths import CONFIG, REPO

SOURCE_ROOTS = ("src", "include/kf")
SOURCE_EXTS = {".c", ".h", ".inc"}
BASELINE = CONFIG / "cleanliness" / "readability-baseline.tsv"
RETAINED_GOTOS = REPO / "docs" / "retained-gotos.tsv"

#: A function longer than this many lines is a split candidate.
LONG_FUNCTION_LINES = 150
#: Block depth inside a function body (the body itself is depth 0).
DEEP_NESTING_DEPTH = 5
#: Consecutive `a[N] = literal;` statements that read as an unrolled table.
ELEMENT_FILL_RUN = 4
#: A literal used in at least this many functions wants one shared name.
REPEATED_VALUE_FUNCTIONS = 4


# --------------------------------------------------------------------------- #
# Interface
# --------------------------------------------------------------------------- #

@dataclass(frozen=True)
class Finding:
    path: str
    line: int
    function: str | None
    text: str


@dataclass(frozen=True)
class Function:
    name: str
    start: int  # offset of the name
    body_start: int  # offset of the opening brace
    body_end: int  # offset of the closing brace
    params: str


class Source:
    """One source file in three views that keep every offset and line.

    ``code`` blanks comments and string/char literals; ``body`` additionally
    blanks everything that is not executable code (see the module docstring),
    and ``function_code`` keeps only function bodies of ``body``.
    """

    def __init__(self, path: str, raw: str) -> None:
        self.path = path
        self.raw = raw
        self.code = _blank_comments_and_strings(raw)
        self.body = _executable_only(self.code)
        self.functions = _function_spans(self.code)
        self.function_code = _only_spans(self.body, ((f.body_start, f.body_end)
                                                     for f in self.functions))
        self._line_starts = [0] + [m.end() for m in re.finditer(r"\n", raw)]
        self._function_starts = [f.start for f in self.functions]

    def line(self, offset: int) -> int:
        return bisect.bisect_right(self._line_starts, offset)

    def function_at(self, offset: int) -> Function | None:
        i = bisect.bisect_right(self._function_starts, offset) - 1
        if i >= 0 and offset <= self.functions[i].body_end:
            return self.functions[i]
        return None

    def finding(self, offset: int, text: str) -> Finding:
        function = self.function_at(offset)
        return Finding(self.path, self.line(offset), function.name if function else None,
                       " ".join(text.split()))


#: A finder yields findings over the whole corpus; most work per file.
Finder = Callable[[Sequence[Source]], Iterator[Finding]]


@dataclass(frozen=True)
class Metric:
    label: str
    gated: bool
    fix: str
    find: Finder


def load_sources(root: Path = REPO) -> list[Source]:
    paths = sorted(p for d in SOURCE_ROOTS for p in (root / d).rglob("*")
                   if p.is_file() and p.suffix in SOURCE_EXTS)
    return [Source(str(p.relative_to(root)), p.read_text(errors="replace")) for p in paths]


def scan(sources: Sequence[Source]) -> dict[str, list[Finding]]:
    return {metric.label: list(metric.find(sources)) for metric in METRICS}


def function_debt(results: dict[str, list[Finding]]) -> list[tuple[str, str, Counter]]:
    """(path, function, findings per gated metric), most findings first."""
    gated = {m.label for m in METRICS if m.gated}
    debt: dict[tuple[str, str], Counter] = defaultdict(Counter)
    for label, findings in results.items():
        if label not in gated:
            continue
        for f in findings:
            if f.function:
                debt[(f.path, f.function)][label] += 1
    ranked = sorted(debt.items(), key=lambda kv: (-sum(kv[1].values()), kv[0]))
    return [(path, name, counts) for (path, name), counts in ranked]


# --------------------------------------------------------------------------- #
# Source views
# --------------------------------------------------------------------------- #

def _blank(text: str) -> str:
    return re.sub(r"[^\n]", " ", text)


_COMMENT_OR_STRING = re.compile(
    r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.DOTALL)


def _blank_comments_and_strings(raw: str) -> str:
    def repl(m: re.Match[str]) -> str:
        s = m.group(0)
        return _blank(s) if s[0] == "/" else s[0] + _blank(s[1:-1]) + s[-1]
    return _COMMENT_OR_STRING.sub(repl, raw)


def _matching_brace(text: str, open_at: int) -> int:
    depth = 0
    for i in range(open_at, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    return len(text) - 1


def _blank_braced(text: str, opener: re.Pattern[str]) -> str:
    """Blank every `opener ... {...}` region, the opener included."""
    out, pos = [], 0
    for m in opener.finditer(text):
        if m.start() < pos:
            continue
        end = _matching_brace(text, m.end() - 1)
        out += [text[pos:m.start()], _blank(text[m.start():end + 1])]
        pos = end + 1
    out.append(text[pos:])
    return "".join(out)


_INITIALIZER = re.compile(r"=\s*\{")
_ENUM_BODY = re.compile(r"\benum\b[^;{()]*\{")
_KF_ENUM_BODY = re.compile(r"\bKF_ENUM_BEGIN\s*\([^)]*\).*?\bKF_ENUM_END\s*\([^)]*\)", re.DOTALL)
_PREPROCESSOR = re.compile(r"^[ \t]*#(?:.*\\\n)*.*$", re.MULTILINE)
_CLAIM = re.compile(r"\b(?:ADDRESS|ADDRESS_AT|DATA|DATA_AT|RODATA|RODATA_AT|BSS)\s*\([^)]*\)")
_LAYOUT_CHECK = re.compile(r"\btypedef\s+char\s+\w+\s*\[[^;]*\];", re.DOTALL)


def _executable_only(code: str) -> str:
    code = _blank_braced(code, _INITIALIZER)
    code = _blank_braced(code, _ENUM_BODY)
    for pattern in (_KF_ENUM_BODY, _PREPROCESSOR, _CLAIM, _LAYOUT_CHECK):
        code = pattern.sub(lambda m: _blank(m.group(0)), code)
    return code


def _only_spans(text: str, spans: Iterable[tuple[int, int]]) -> str:
    out, pos = [], 0
    for start, end in spans:
        out += [_blank(text[pos:start]), text[start:end + 1]]
        pos = end + 1
    out.append(_blank(text[pos:]))
    return "".join(out)


_FUNCTION_HEAD = re.compile(r"\b([A-Za-z_]\w*)\s*\(((?:[^(){};]|\([^(){};]*\))*)\)\s*$")
_NOT_FUNCTIONS = {"if", "for", "while", "switch", "return", "sizeof",
                  "KF_ENUM_BEGIN", "KF_ENUM_FLAGS", "KF_ENUM_COUNTER"}


def _function_spans(code: str) -> list[Function]:
    """Top-level `name(params) {` definitions, found by brace depth."""
    functions, depth, statement_start = [], 0, 0
    i = 0
    while i < len(code):
        ch = code[i]
        if ch == "{" and depth == 0:
            head = code[statement_start:i]
            m = _FUNCTION_HEAD.search(head)
            end = _matching_brace(code, i)
            if m and m.group(1) not in _NOT_FUNCTIONS and "=" not in head[:m.start()]:
                functions.append(Function(m.group(1), statement_start + m.start(1), i, end,
                                          m.group(2)))
            i, statement_start = end + 1, end + 1
            continue
        if ch in ";}" and depth == 0:
            statement_start = i + 1
        i += 1
    return functions


def _per_file(find: Callable[[Source], Iterable[Finding]]) -> Finder:
    def run(sources: Sequence[Source]) -> Iterator[Finding]:
        for source in sources:
            yield from find(source)
    return run


def _regex(pattern: str, view: str = "function_code", flags: int = 0) -> Finder:
    """Every match of `pattern` in one view of each file."""
    compiled = re.compile(pattern, flags)

    def find(source: Source) -> Iterator[Finding]:
        for m in compiled.finditer(getattr(source, view)):
            yield source.finding(m.start(), m.group(0))
    return _per_file(find)


# --------------------------------------------------------------------------- #
# Names
# --------------------------------------------------------------------------- #

#: word_10, byte_2c, unknown_02, unk1B4, field_08: a name that is only an offset.
OFFSET_NAME = r"(?:word|byte|half|dword|field|unknown|unk)_?[0-9a-fA-F]{1,4}"
_OFFSET_MEMBER_DECL = re.compile(
    r"^[ \t]+[\w \t\*\(\),]+?\b(" + OFFSET_NAME + r")\s*(?:\[[^\]]*\]\s*)*;", re.MULTILINE)
_OFFSET_TYPE_DECL = re.compile(
    r"}\s*(\w*?(?:Word|Byte|Half|Dword|Field|Unk)[0-9a-fA-F]{2,4}\w*)\s*;")


@_per_file
def _offset_named_members(source: Source) -> Iterator[Finding]:
    for m in _OFFSET_MEMBER_DECL.finditer(source.code):
        yield source.finding(m.start(1), m.group(1))


@_per_file
def _offset_named_types(source: Source) -> Iterator[Finding]:
    for m in _OFFSET_TYPE_DECL.finditer(source.code):
        yield source.finding(m.start(1), m.group(1))


_ENUM_BLOCK = re.compile(r"\bKF_ENUM_BEGIN\s*\([^)]*\)(.*?)\bKF_ENUM_END\b"
                         r"|\benum\b[^;{()]*\{(.*?)\}", re.DOTALL)
_ENUMERATOR = re.compile(r"\b([A-Z][A-Z0-9_]*)\s*(?:=\s*([^,]+?))?\s*(?:,|$)")
_DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Z][A-Z0-9_]*)[ \t]+"
                     r"\(?([-+]?(?:0x[0-9A-Fa-f]+|\d+))[uUlL]*\)?[ \t]*$", re.MULTILINE)
_NUMBER_SUFFIX = re.compile(r"_(0x[0-9A-Fa-f]+|[0-9A-F]*\d[0-9A-F]*)$")


def _integer(text: str | None) -> int | None:
    if text is None:
        return None
    m = re.fullmatch(r"\(?\s*([-+]?(?:0x[0-9A-Fa-f]+|\d+))[uUlL]*\s*\)?", text.strip())
    return int(m.group(1), 0) if m else None


#: Families whose number is the meaning: KF_QUARTER_TURN_2 is two quarter turns.
COUNTED_NAMES = ("KF_QUARTER_TURN_",)


def echoes_value(name: str, value: int) -> bool:
    """Whether a constant's name only repeats its value (KF_EFFECT_KIND_102 = 102)."""
    m = _NUMBER_SUFFIX.search(name)
    if not m or name.startswith(COUNTED_NAMES):
        return False
    digits = m.group(1)
    readings = {int(digits, 16)}
    if digits.isdigit():
        readings.add(int(digits, 10))
    return value in readings


def _enumerators(body: str) -> Iterator[tuple[int, str, int | None]]:
    """(offset in body, name, value) with implicit values counted forward."""
    value: int | None = -1
    for m in _ENUMERATOR.finditer(body):
        explicit = m.group(2)
        value = _integer(explicit) if explicit is not None else (
            None if value is None else value + 1)
        yield m.start(1), m.group(1), value


@_per_file
def _value_echo_constants(source: Source) -> Iterator[Finding]:
    for block in _ENUM_BLOCK.finditer(source.code):
        group = 1 if block.group(1) is not None else 2
        for offset, name, value in _enumerators(block.group(group)):
            if value is not None and echoes_value(name, value):
                yield source.finding(block.start(group) + offset, name)
    for m in _DEFINE.finditer(source.code):
        if echoes_value(m.group(1), int(m.group(2), 0)):
            yield source.finding(m.start(1), m.group(1))


@_per_file
def _numbered_functions(source: Source) -> Iterator[Finding]:
    for f in source.functions:
        if re.search(r"_\d+$", f.name) and not re.match(r"(?:func|FUN)_", f.name):
            yield source.finding(f.start, f.name)


_PLACEHOLDER_PARAM = re.compile(
    r"\b(arg_?\d+|param_?\d+|unknown\w*|unk\w*|a[0-3])\s*(?:\[[^\]]*\]\s*)*$")


@_per_file
def _placeholder_parameters(source: Source) -> Iterator[Finding]:
    for f in source.functions:
        offset = source.code.index("(", f.start) + 1
        for param in f.params.split(","):
            m = _PLACEHOLDER_PARAM.search(param.strip())
            if m:
                yield source.finding(offset, m.group(1))
            offset += len(param) + 1


# --------------------------------------------------------------------------- #
# Literals
# --------------------------------------------------------------------------- #

_NUMBER = r"-?(?:0x[0-9A-Fa-f]+|\d+)[uUlL]*\b(?![.\w])"
#: Members named like states that hold a continuous value instead: q12 cycle
#: positions and oscillator angles, whose zero is a position, not a state.
#: A `*_step` member is an increment; only a bare `step` names a state.
CONTINUOUS_PHASE_FIELDS = ("animation_phase", "walking_bob_phase", "magic_tint_phase",
                           "darkness_phase", "secondary_hit_phase")
_STATE_FIELD = (r"(?:->|\.)\s*(?!(?:" + "|".join(CONTINUOUS_PHASE_FIELDS) + r")\b)"
                r"(?:\w*(?:state|mode|kind|phase|action|type)|step)\b")
_BIT_OP = r"(?<=[\w)\]])\s*(?<![&|])[&|^]=?(?![&|=])\s*~?\s*\(?\s*"


@_per_file
def _raw_bit_masks(source: Source) -> Iterator[Finding]:
    """Masks other than 2^k - 1 (a truncation, not a flag)."""
    for m in re.finditer(_BIT_OP + r"(0x[0-9A-Fa-f]+|\d+)\b(?![.\w])", source.function_code):
        n = int(m.group(1), 0)
        if n and n & (n + 1):
            yield source.finding(m.start(1), m.group(0))


_ELEMENT_FILL = re.compile(r"^\s*[\w.\->\[\]]+\[\s*\d+\s*\]\s*=\s*" + _NUMBER + r"\s*;\s*$")


@_per_file
def _literal_element_fills(source: Source) -> Iterator[Finding]:
    """One finding per run of consecutive `a[N] = literal;` statements."""
    run_start, run = None, 0
    lines = source.function_code.split("\n") + [""]
    offset = 0
    for text in lines:
        if _ELEMENT_FILL.match(text):
            if run == 0:
                run_start = offset
            run += 1
        else:
            if run >= ELEMENT_FILL_RUN and run_start is not None:
                yield source.finding(run_start, f"{run} element stores")
            run = 0
        offset += len(text) + 1


_LITERAL = re.compile(r"(?<![\w.])(0x[0-9A-Fa-f]+|\d+)[uUlL]*\b(?![.\w])")
_TRIVIAL_VALUES = {0, 1, 2, 10, 100}


def _repeated_values(sources: Sequence[Source]) -> Iterator[Finding]:
    """Sites of literals that several functions spell independently."""
    sites: dict[int, list[tuple[Source, int, str]]] = defaultdict(list)
    for source in sources:
        for f in source.functions:
            for m in _LITERAL.finditer(source.function_code, f.body_start, f.body_end):
                value = int(m.group(1), 0)
                if value not in _TRIVIAL_VALUES and value > 8:
                    sites[value].append((source, m.start(), f.name))
    for value, where in sorted(sites.items()):
        if len({(s.path, name) for s, _o, name in where}) >= REPEATED_VALUE_FUNCTIONS:
            for source, offset, _name in where:
                yield source.finding(offset, f"{value:#x} ({value})")


# --------------------------------------------------------------------------- #
# Control flow
# --------------------------------------------------------------------------- #

def load_retained_gotos(path: Path | None = None) -> set[tuple[str, str, str]]:
    """(file, function, label) rows of the curated goto inventory."""
    path = path or RETAINED_GOTOS
    if not path.is_file():
        return set()
    rows = set()
    for line in path.read_text().splitlines():
        cols = line.split("\t")
        if line.startswith("#") or len(cols) < 3 or cols[0] == "file":
            continue
        rows.add((cols[0], cols[1], cols[2]))
    return rows


def _unlisted_gotos(sources: Sequence[Source]) -> Iterator[Finding]:
    retained = load_retained_gotos()
    for source in sources:
        for m in re.finditer(r"\bgoto\s+(\w+)", source.function_code):
            finding = source.finding(m.start(), m.group(0))
            if (source.path, finding.function, m.group(1)) not in retained:
                yield finding


def _max_depth(code: str, start: int, end: int) -> int:
    depth = deepest = 0
    for ch in code[start:end + 1]:
        if ch == "{":
            depth += 1
            deepest = max(deepest, depth)
        elif ch == "}":
            depth -= 1
    return deepest - 1


@_per_file
def _long_functions(source: Source) -> Iterator[Finding]:
    for f in source.functions:
        lines = source.line(f.body_end) - source.line(f.body_start) + 1
        if lines > LONG_FUNCTION_LINES:
            yield source.finding(f.start, f"{f.name}: {lines} lines")


@_per_file
def _deeply_nested_functions(source: Source) -> Iterator[Finding]:
    for f in source.functions:
        depth = _max_depth(source.code, f.body_start, f.body_end)
        if depth >= DEEP_NESTING_DEPTH:
            yield source.finding(f.start, f"{f.name}: depth {depth}")


# --------------------------------------------------------------------------- #
# Comments
# --------------------------------------------------------------------------- #

#: Headers whose comments document the address/claim machinery itself.
CLAIM_DOCUMENTATION = {"include/kf/lib/address.h"}


def _comment_regex(pattern: str) -> Finder:
    compiled = re.compile(pattern, re.IGNORECASE)

    def find(source: Source) -> Iterator[Finding]:
        if source.path in CLAIM_DOCUMENTATION:
            return
        for comment in re.finditer(r"/\*.*?\*/|//[^\n]*", source.raw, re.DOTALL):
            if source.code[comment.start()] != " ":
                continue  # inside a string literal
            for m in compiled.finditer(comment.group(0)):
                yield source.finding(comment.start() + m.start(), m.group(0))
    return _per_file(find)


# --------------------------------------------------------------------------- #
# The table
# --------------------------------------------------------------------------- #

METRICS: tuple[Metric, ...] = (
    # Names
    Metric("offset-named members", True,
           "name the member for what it holds; split a union by its uses",
           _offset_named_members),
    Metric("offset-named member uses", True,
           "falls as the members above get names",
           _regex(r"(?:->|\.)\s*" + OFFSET_NAME + r"\b")),
    Metric("offset-named types", True,
           "name the type after the member's meaning (KfTargetCandidateWord0c)",
           _offset_named_types),
    Metric("value-echo constants", True,
           "an enumerator that repeats its value (KIND_102 = 102) names nothing",
           _value_echo_constants),
    Metric("numbered functions", True,
           "name each variant for what differs (set_phase_1, play_sound_64)",
           _numbered_functions),
    Metric("placeholder parameters", True,
           "name the parameter from its uses and call sites (arg3, unknown)",
           _placeholder_parameters),
    # Literals
    Metric("raw case labels", True,
           "switch on an enum and label each case by name",
           _regex(r"\bcase\s+" + _NUMBER + r"\s*(?::|\.\.\.)")),
    Metric("raw state values", True,
           "assign and compare state/mode/kind/phase fields with enum names",
           _regex(_STATE_FIELD + r"\s*(?:==|!=|=(?!=))\s*" + _NUMBER)),
    Metric("raw bit masks", True,
           "name the flag bit (KF_ACTOR_FLAG_...)", _raw_bit_masks),
    Metric("raw hex literals", True,
           "name the constant, or derive it (sizeof, a shift, a field width)",
           _regex(r"(?<![\w.])0x[0-9A-Fa-f]+[uUlL]*\b")),
    Metric("literal element fills", True,
           "an unrolled table: a static table or string if the match allows",
           _literal_element_fills),
    Metric("repeated magic values", False,
           "a value several functions spell alike is one named constant",
           _repeated_values),
    Metric("raw shift amounts", False,
           "fixed-point shifts read better as a named scale (Q12)",
           _regex(r"(?:<<|>>)=?\s*\d+\b")),
    # Control flow
    Metric("gotos", True,
           "rewrite as structured flow where the match allows", _regex(r"\bgoto\b")),
    Metric("unlisted gotos", True,
           "rewrite it, or record why it stays in docs/retained-gotos.tsv",
           _unlisted_gotos),
    Metric("long functions", True,
           f"over {LONG_FUNCTION_LINES} lines: extract helpers if the match allows",
           _long_functions),
    Metric("deeply nested functions", True,
           f"block depth {DEEP_NESTING_DEPTH}+: early returns, extracted cases",
           _deeply_nested_functions),
    # Views
    Metric("union scalar views", True,
           "read the member that means something, not .value/.bytes of a word",
           _regex(r"(?:->|\.)\s*(?:value|bytes|halves|words|raw)\b(?!\s*\()")),
    Metric("negative indices", False,
           "index from the owning object, not backwards from an interior one",
           _regex(r"\[\s*-\s*\d+\s*\]")),
    # Comments
    Metric("matching-history comments", True,
           "comments say what the code does, not how it was matched",
           _comment_regex(r"\b(?:objdiff|permuter|m2c|regalloc|decomp(?:iler|\.me)?|"
                          r"fake|match(?:es|ed|ing)? (?:retail|score|percent)|\d+(?:\.\d+)?%)")),
    Metric("addresses in comments", True,
           "the claim already records the address; refer to the name",
           _comment_regex(r"\b0x80[0-9a-fA-F]{6}\b")),
)


# --------------------------------------------------------------------------- #
# Ratchet and report
# --------------------------------------------------------------------------- #

def totals(results: dict[str, list[Finding]]) -> list[tuple[str, int]]:
    return [(m.label, len(results[m.label])) for m in METRICS]


def load_baseline(path: Path | None = None) -> dict[str, int]:
    out: dict[str, int] = {}
    path = path or BASELINE
    if not path.is_file():
        return out
    for line in path.read_text().splitlines():
        if line.startswith("#") or "\t" not in line:
            continue
        label, value = line.rsplit("\t", 1)
        if value.strip().isdigit():
            out[label] = int(value)
    return out


def save_baseline(rows: Sequence[tuple[str, int]], path: Path | None = None) -> None:
    path = path or BASELINE
    gated = {m.label for m in METRICS if m.gated}
    header = (
        "# kf.readability - committed floors for the source readability ratchet.\n"
        "# RATCHET: down-only. Bless a lower number with `kf verify readability --update`;\n"
        "# a higher one fails `kf verify readability --gate`.\n"
        "# Never hand-edit to raise a floor - rewrite the new site instead.\n"
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(header + "".join(f"{label}\t{n}\n" for label, n in rows if label in gated))


def gate(rows: Sequence[tuple[str, int]], base: dict[str, int]) -> list[str]:
    """Gated metrics above their floor; a missing floor fails closed."""
    gated = {m.label for m in METRICS if m.gated}
    bad = [f"{label}: no committed floor" for label, _n in rows
           if label in gated and label not in base]
    bad += [f"{label}: {base[label]} -> {n} (+{n - base[label]})" for label, n in rows
            if label in gated and label in base and n > base[label]]
    return bad


def report_lines(rows: Sequence[tuple[str, int]], base: dict[str, int]) -> list[str]:
    gated = {m.label for m in METRICS if m.gated}
    width = max(len(label) for label, _n in rows)
    lines = ["readability (gated metrics against committed floors; lower is better):"]
    for label, n in rows:
        if label not in gated:
            tag = "  [informational]"
        elif label not in base:
            tag = "  [NO FLOOR]"
        else:
            delta = n - base[label]
            tag = f"  ({delta:+d})" if delta else "  (=)"
        lines.append(f"    {label:<{width}}  {n:>6}{tag}")
    return lines


def _print_functions(results: dict[str, list[Finding]], limit: int) -> None:
    for path, name, counts in function_debt(results)[:limit]:
        detail = ", ".join(f"{label} {n}" for label, n in counts.most_common())
        print(f"{sum(counts.values()):5}  {path}  {name}  ({detail})")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="kf verify readability", description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--gate", action="store_true",
                        help="exit 1 when a gated metric rose above its floor")
    action.add_argument("--update", action="store_true",
                        help="MANUAL bless: rewrite the committed floor file")
    action.add_argument("--list", metavar="LABEL", help="print every site of one metric")
    action.add_argument("--functions", nargs="?", type=int, const=30, metavar="N",
                        help="the N functions with the most gated findings")
    action.add_argument("--metrics", action="store_true", help="describe each metric")
    args = parser.parse_args(argv)

    if args.metrics:
        for m in METRICS:
            print(f"{m.label:<26} {'gated' if m.gated else 'info ':5}  {m.fix}")
        return 0
    if args.list and args.list not in {m.label for m in METRICS}:
        print(f"unknown metric {args.list!r}; see --metrics", file=sys.stderr)
        return 2

    results = scan(load_sources())
    if args.list:
        for f in results[args.list]:
            print(f"{f.path}:{f.line}\t{f.function or '-'}\t{f.text}")
        print(f"# {len(results[args.list])} {args.list}")
        return 0
    if args.functions is not None:
        _print_functions(results, args.functions)
        return 0

    rows = totals(results)
    if args.update:
        save_baseline(rows)
        print(f"readability baseline updated ({BASELINE.relative_to(REPO)})")
        return 0
    base = load_baseline()
    for line in report_lines(rows, base):
        print(line)
    bad = gate(rows, base)
    for finding in bad:
        print(f"readability RATCHET VIOLATED: {finding}", file=sys.stderr)
    return 1 if bad and args.gate else 0


if __name__ == "__main__":
    raise SystemExit(main())
