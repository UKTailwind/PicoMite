import json
import pathlib
import re
from collections import defaultdict
from typing import Dict, List, Set, Tuple

ROOT = pathlib.Path(__file__).resolve().parents[1]
ALLCMDS = ROOT.parent / "AllCommands.h"
SNIPPETS = ROOT / "snippets" / "mmbasic.code-snippets"

COMMAND_ENTRY_RE = re.compile(r"\{\(unsigned char \*\)\"([^\"]+)\"\s*,\s*T_CMD[^}]*?,\s*cmd_(\w+)\}")
TOKEN_RE = re.compile(r"\{\(unsigned char \*\)\"([^\"]+)\"\s*,\s*T_(?:OPER|FUN|FNA|NA)")
CMD_FUNC_RE = re.compile(r"void\s+\w*\s*cmd_(\w+)\s*\(")
CHECKSTRING_RE = re.compile(r"checkstring\(cmdline\s*,\s*\(unsigned char \*\)\"([^\"]+)\"")


def parse_commands() -> List[Tuple[str, str]]:
    text = ALLCMDS.read_text()
    return COMMAND_ENTRY_RE.findall(text)


def parse_functions() -> List[str]:
    text = ALLCMDS.read_text()
    return TOKEN_RE.findall(text)


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


def make_snippet(name: str, description: str) -> Dict[str, object]:
    return {
        "prefix": name.lower(),
        "body": [name],
        "description": description,
    }


def main() -> None:
    commands = parse_commands()
    tokens = parse_functions()
    subcommands = parse_subcommands()

    # map fn -> names
    fn_to_cmdnames = defaultdict(list)
    for cname, fn in commands:
        fn_to_cmdnames[fn].append(cname)

    # functions are the tokens that contain at least one alpha
    functions = [t for t in tokens if re.search(r"[A-Za-z]", t)]

    snippets: Dict[str, object] = {}

    for cmd, fn in commands:
        snippets[f"cmd: {cmd}"] = make_snippet(cmd, f"Command {cmd}")
        for sub in sorted(subcommands.get(fn, [])):
            combo = f"{cmd} {sub}"
            snippets[f"cmd: {combo}"] = make_snippet(combo, f"Command {combo}")

    for fun in functions:
        snippets[f"fun: {fun}"] = make_snippet(fun, f"Function {fun}")

    SNIPPETS.write_text(json.dumps(snippets, indent=2))
    print(f"Wrote {SNIPPETS}")


if __name__ == "__main__":
    main()
