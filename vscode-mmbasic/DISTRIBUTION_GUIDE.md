# MMBasic VS Code Extension – Install & Use (VSIX)

This doc is for anyone you share the VSIX with. It covers install, setup, and basic use on Windows/macOS/Linux.

## 1) Install the VSIX
- In VS Code: **Ctrl+Shift+P** → “Extensions: Install from VSIX…” → select `mmbasic-support-<version>.vsix`.
- Or terminal: `code --install-extension mmbasic-support-<version>.vsix`.

## 2) Select language & theme (one-time)
- Language mode: click the status bar language indicator → choose **MMBasic** (or Command Palette → “Change Language Mode” → MMBasic).
- Color theme: **Ctrl+K Ctrl+T** → choose **MMBasic Editor**.

## 3) Upload shortcut
- Focus an MMBasic file and press **Ctrl+Alt+U** (Windows/Linux) or **Cmd+Alt+U** (macOS) to run “MMBasic: Send To PicoMite”.
- Pick your serial port (e.g., COM3). Adjust in Settings if needed:
  - `mmbasicUploader.baud` (default 115200)
  - `mmbasicUploader.lineDelayMs` (default 5 ms)

## 4) Serial console
- Run “MMBasic: Open Console” (Command Palette or command list), choose the port. A terminal tab opens.
- If the PicoMite restarts (e.g., after OPTION changes), the console auto-reconnects and stays open.

## 5) Notes
- The VSIX is platform-agnostic; use the same file on Windows/macOS/Linux.
- If `serialport` needs to rebuild, ensure build tools are present (Windows: recent Node.js usually has prebuilds; Linux: build-essential python3 make; macOS: Xcode Command Line Tools).
- To update, just reinstall with a newer VSIX.

## 6) Quick reminder
1) Install from VSIX
2) Set language: MMBasic
3) Set theme: MMBasic Editor
4) Upload: Ctrl+Alt+U (Cmd+Alt+U on macOS)
5) Console: “MMBasic: Open Console”
