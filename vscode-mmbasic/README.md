# MMBasic VS Code support

This extension adds PicoMite-style syntax colouring for MMBasic, using the same palette as the built-in editor in `Editor.c` (white text on black, cyan keywords, yellow comments, magenta strings, green numbers).

## What is included
- Language registration for `.bas`, `.mmb`, `.mmba` files.
- TextMate grammar generated from `AllCommands.h` command and token tables, including two-word commands (eg. `END FUNCTION`, `SELECT CASE`) and dual-purpose tokens such as `MID$(`.
- Colour theme `MMBasic Editor` that maps the Draw.h palette (WHITE, BLACK, CYAN, YELLOW, MAGENTA, GREEN) to VS Code scopes.
- Comment support for `'`, `REM`, and `/* ... */`, plus number detection for decimal/hex/bin/octal literals.

## How to use locally
1. In VS Code, run `Developer: Reload Window` after creating the files (or restart VS Code).
2. Choose `Extensions: Install from VSIX...` and point to a packaged `.vsix`, or run VS Code in extension development mode:
   - `code --extensionDevelopmentPath d:\Dropbox\PicoMite\PicoMite\vscode-mmbasic`
3. Switch to the `MMBasic Editor` colour theme if you want the PicoMite palette.

## Regenerating the keyword list
The grammar comes from `AllCommands.h`. If that file changes, rerun the helper script below from the workspace root to rebuild the grammar file:

```powershell
@'
import re, pathlib, json

data = pathlib.Path("AllCommands.h").read_text()
commands = re.findall(r'\{\(unsigned char \*\)"([^"]+)"\s*,\s*T_CMD', data)
tokens = re.findall(r'\{\(unsigned char \*\)"([^"]+)"\s*,\s*T_(?:OPER|FUN|FNA|NA)', data)

def clean(name: str) -> str:
    pat = re.escape(name)
    pat = pat.replace(r"\ ", r"\\s+")
    pat = pat.replace(r"\\\(", r"\\s*\\(")
    return pat

command_pattern = "(?i)\\b(?:" + "|".join(clean(c) for c in sorted(set(commands))) + ")\\b"
func_tokens = [t for t in sorted(set(tokens)) if re.search(r"[A-Za-z]", t)]
function_pattern = "(?i)\\b(?:" + "|".join(clean(t) for t in func_tokens) + ")\\b"

grammar = pathlib.Path('vscode-mmbasic/syntaxes/mmbasic.tmLanguage.json')
grammar.write_text(json.dumps({
    "name": "MMBasic",
    "scopeName": "source.mmbasic",
    "patterns": [
        {"include": "#comments"},
        {"include": "#strings"},
        {"include": "#numbers"},
        {"include": "#keywords"},
        {"include": "#functions"}
    ],
    "repository": {
        "comments": {"patterns": [
            {"name": "comment.block.mmbasic", "begin": "/\\*", "end": "\\*/"},
            {"name": "comment.line.apostrophe.mmbasic", "match": "'.*$"},
            {"name": "comment.line.rem.mmbasic", "match": "(?i)^\\s*REM\\b.*$"}
        ]},
        "strings": {"name": "string.quoted.double.mmbasic", "begin": "\"", "end": "\"",
            "patterns": [{"name": "constant.character.escape.mmbasic", "match": "\"\""}]},
        "numbers": {"patterns": [{"name": "constant.numeric.mmbasic",
            "match": "(?i)(?<![\\\\w$])(?:[+-]?(?:\\\\d+(?:\\\\.\\\\d*)?|\\\\.\\\\d+)(?:[eE][+-]?\\\\d+)?|&[hH][0-9a-f]+|&[bB][01]+|&[oO][0-7]+)"}]},
        "keywords": {"patterns": [{"name": "keyword.control.mmbasic", "match": command_pattern}]},
        "functions": {"patterns": [{"name": "support.function.mmbasic", "match": function_pattern}]}
    }
}, indent=2))
print("Updated grammar")
'@ | python -
```

## Notes
- The token colours mirror `GUI_C_*` and `VT100_C_*` mappings in `Editor.c` and the colour constants in `Draw.h`.
- Keyword matching is case-insensitive and permits the same flexible spacing used by the editor (eg. `_side set`, `Else If`).
- Numbers match decimal, hex (`&H`), binary (`&B`), and octal (`&O`) formats used by MMBasic.
