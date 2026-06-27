# Building the MMBasic VS Code Extension on Linux and macOS

This guide covers packaging the extension into a VSIX and installing it locally.

## Prerequisites
- Node.js 18+ and npm installed and on PATH (`node -v`, `npm -v`).
- VS Code installed (`code` on PATH for `--install-extension`).
- VSCE (the VS Code packaging CLI): `npm install -g @vscode/vsce` (or use `npx vsce ...`).
- Git (for cloning, if needed).

### Platform toolchains
Serialport may compile native bits if a prebuilt binary is not available.
- **Linux**: `sudo apt-get install build-essential python3 make` (Debian/Ubuntu) or equivalent toolchain for your distro.
- **macOS**: `xcode-select --install` to get Command Line Tools (clang, make, headers).

## Steps (Linux and macOS)
1) Get the source folder
- With git: `git clone <repo-url>` then `cd vscode-mmbasic`
- Without git: download/unzip the source archive (or copy the folder you already have) so you have the `package.json` at the root.

2) Install dependencies
```sh
npm install
```

3) Package the extension
```sh
vsce package
# or
npx vsce package
```
This produces a `mmbasic-support-<version>.vsix` file in the project root.

4) Install the VSIX into VS Code
```sh
code --install-extension mmbasic-support-<version>.vsix
```

5) (Optional) Run the extension in the debugger
- Open the folder in VS Code
- Press `F5` to launch an Extension Development Host.

## Notes
- If `serialport` triggers a build and fails, ensure the platform toolchain prerequisites above are installed, then rerun `npm install`.
- For offline packaging, install `vsce` once (`npm install -g @vscode/vsce`), then you can run `vsce package` without network access except for npm dependency resolution.
