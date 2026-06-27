import json
import pathlib
import re
from collections import defaultdict
from typing import Dict, List, Set, Tuple

ROOT = pathlib.Path(__file__).resolve().parents[1]
ALLCMDS = ROOT.parent / "AllCommands.h"
GRAMMAR_OUT = ROOT / "syntaxes" / "mmbasic.tmLanguage.json"

COMMAND_ENTRY_RE = re.compile(r"\{\(unsigned char \*\)\"([^\"]+)\"\s*,\s*T_CMD[^}]*?,\s*cmd_(\w+)\}")
TOKEN_FUN_RE = re.compile(r"\{\(unsigned char \*\)\"([^\"]+)\"\s*,\s*T_(?:OPER|FUN|FNA|NA)")
CMD_FUNC_RE = re.compile(r"void\s+\w*\s*cmd_(\w+)\s*\(")
CHECKSTRING_RE = re.compile(r"checkstring\(cmdline\s*,\s*\(unsigned char \*\)\"([^\"]+)\"")


def clean_name(name: str) -> str:
    # Allow flexible spacing and optional space before (
    pat = re.escape(name)
    pat = pat.replace(r"\ ", r"\\s+")
    pat = pat.replace(r"\\\(", r"\\s*\\(")
    return pat


def clean_sub(name: str) -> str:
    pat = re.escape(name)
    pat = pat.replace(r"\ ", r"\\s+")
    return pat


def parse_commands() -> List[Tuple[str, str]]:
    text = ALLCMDS.read_text()
    return COMMAND_ENTRY_RE.findall(text)


def parse_functions() -> List[str]:
    text = ALLCMDS.read_text()
    return TOKEN_FUN_RE.findall(text)


def parse_subcommands() -> Dict[str, Set[str]]:
    sub = defaultdict(set)
    for cfile in ROOT.parent.glob("*.c"):
        text = cfile.read_text(errors="ignore")
        current = None
        for line in text.splitlines():
            mcmd = CMD_FUNC_RE.search(line)
            if mcmd:
                current = mcmd.group(1)
            msub = CHECKSTRING_RE.search(line)
            if msub and current:
                sub[current].add(msub.group(1))
    return sub


def build_grammar(commands: List[Tuple[str, str]], functions: List[str], subcommands: Dict[str, Set[str]]) -> Dict:
    command_names = [c for c, _ in commands]
    command_pattern = "(?i)\\b(?:" + "|".join(clean_name(c) for c in sorted(set(command_names))) + ")\\b"

    func_tokens = [t for t in sorted(set(functions)) if re.search(r"[A-Za-z]", t)]
    function_pattern = "(?i)\\b(?:" + "|".join(clean_name(t) for t in func_tokens) + ")\\b"

    # Map cmd_fn -> display names (some commands share handler)
    fn_to_cmdnames = defaultdict(list)
    for name, fn in commands:
        fn_to_cmdnames[fn].append(name)

    sub_patterns = []
    for fn, subs in subcommands.items():
        if not subs:
            continue
        cmdnames = fn_to_cmdnames.get(fn)
        if not cmdnames:
            continue
        sub_pat = "|".join(clean_sub(s) for s in sorted(subs))
        for cname in cmdnames:
            cmd_pat = clean_name(cname)
            sub_patterns.append(
                {
                    "name": "meta.subcommand.mmbasic",
                    "match": f"(?i)(\\b{cmd_pat}\\s+)({sub_pat})",
                    "captures": {
                        "1": {"name": "keyword.control.mmbasic"},
                        "2": {"name": "support.constant.mmbasic.subcommand"},
                    },
                }
            )

    grammar = {
        "name": "MMBasic",
        "scopeName": "source.mmbasic",
        "patterns": [
            {"include": "#comments"},
            {"include": "#strings"},
            {"include": "#numbers"},
            {"include": "#subcommands"},
            {"include": "#keywords"},
            {"include": "#functions"},
        ],
        "repository": {
            "comments": {
                "patterns": [
                    {
                        "name": "comment.block.mmbasic",
                        "begin": "/\\*",
                        "end": "\\*/",
                        "beginCaptures": {"0": {"name": "punctuation.definition.comment.mmbasic"}},
                        "endCaptures": {"0": {"name": "punctuation.definition.comment.mmbasic"}},
                    },
                    {"name": "comment.line.apostrophe.mmbasic", "match": "'.*$"},
                    {"name": "comment.line.rem.mmbasic", "match": "(?i)^\\s*REM\\b.*$"},
                ]
            },
            "strings": {
                "name": "string.quoted.double.mmbasic",
                "begin": "\"",
                "end": "\"",
                "beginCaptures": {"0": {"name": "punctuation.definition.string.begin.mmbasic"}},
                "endCaptures": {"0": {"name": "punctuation.definition.string.end.mmbasic"}},
                "patterns": [
                    {"name": "constant.character.escape.mmbasic", "match": "\"\""}
                ],
            },
            "numbers": {
                "patterns": [
                    {
                        "name": "constant.numeric.mmbasic",
                        "match": "(?i)(?<![\\\\w$])(?:[+-]?(?:\\\\d+(?:\\\\.\\\\d*)?|\\\\.\\\\d+)(?:[eE][+-]?\\\\d+)?|&[hH][0-9a-f]+|&[bB][01]+|&[oO][0-7]+)",
                    }
                ]
            },
            "subcommands": {
                "patterns": sub_patterns,
            },
            "keywords": {
                "patterns": [
                    {"name": "keyword.control.mmbasic", "match": command_pattern}
                ]
            },
            "functions": {
                "patterns": [
                    {"name": "support.function.mmbasic", "match": function_pattern}
                ]
            },
        },
    }
    return grammar


def main() -> None:
    commands = parse_commands()
    functions = parse_functions()
    subcommands = parse_subcommands()
    grammar = build_grammar(commands, functions, subcommands)
    GRAMMAR_OUT.write_text(json.dumps(grammar, indent=2))
    print(f"Wrote {GRAMMAR_OUT}")


if __name__ == "__main__":
    main()
