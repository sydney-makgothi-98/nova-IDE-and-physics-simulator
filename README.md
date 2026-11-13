# NOVA IDE

In-house C/C++ IDE (Electron + Monaco) following the JetBrains workflow and look. Free to use internally.

**Run:** double-click `Launch NOVA.bat`, or `npm start`. First time on a fresh checkout: `npm install`
(if Electron's binary is missing afterwards: `node node_modules/electron/install.js`).

**Toolchain (MSYS2 mingw64):** `pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gdb mingw-w64-x86_64-clang-tools-extra`
(g++/gcc compile, CMake+Ninja build, gdb debug, clangd + clang-format for code intelligence). NOVA detects whatever is on PATH.

## Features
- **Editor:** tabs, breadcrumbs, structure view, minimap, sticky scroll, C/C++/CMake highlighting, snippets, bracket colorization
- **Code intelligence (clangd / LSP):** completion, hover docs, signature help, go to definition (Ctrl+click / F12),
  find references (Shift+F12), rename symbol (F2), quick fixes (Ctrl+.), live diagnostics, format document (Ctrl+Alt+L), symbol outline
- **Build & run:** CMake projects (auto-detects `add_executable` targets) and single files; clickable compiler diagnostics,
  squiggles, Problems panel; interactive Run console with stdin
- **Debugger (gdb / GDB-MI):** gutter breakpoints (add/remove live while running), step over/into/out (F8/F7/Shift+F8),
  resume (F9), run to cursor (Alt+F9), call stack, variables (changed values highlighted), watches, hover-to-evaluate,
  gdb console (type any gdb command), current-line highlight, crash/signal reporting
- **Terminal:** real PTY (ConPTY) via xterm.js — full interactive shell, colours, TUI programs, copy/paste (Ctrl+Shift+C/V)
- **Navigation:** Search Everywhere (double Shift), Actions palette, Go to file/symbol, Find in Files
- **Project:** New Project wizard (CMake C / C++), recent projects, dark (Darcula-style) & light themes, settings

## Shortcuts
Shift+F10 run · Shift+F9 debug · Ctrl+F9 build · Ctrl+F2 stop · F8/F7/Shift+F8 step · F9 resume · Alt+F9 run to cursor · Ctrl+F8 toggle breakpoint ·
Double-Shift search everywhere · Ctrl+Shift+A actions · Ctrl+P go to file · Ctrl+F12 file symbols · Ctrl+Shift+F find in files ·
Alt+1/7 side panels · Alt+4/5/6/F12 run/debug/problems/terminal · Ctrl+S / Ctrl+Shift+S save · Ctrl+W close tab · Ctrl+Alt+S settings

## Notes & limits
- Debugging runs the program in its own console window by default (so programs that read input work). Untick
  *Settings → Debug: run in own console* to see output inline in the Debug console instead (no program input in that mode).
- Variables are shown as gdb's value text (structs/arrays inline); expandable variable trees are a possible next step.
- Pausing a running program (interrupt) isn't supported on Windows yet; breakpoints and stepping are.
- clangd gets flags from CMake's `compile_commands.json` (NOVA configures `cmake-build-debug/` for you), or from a database NOVA
  generates in `.nova/` for loose files.
- Dev aid: `NOVA_SELFTEST=<script.js> NOVA_SHOT=<out.png> electron .` runs a script in the renderer, saves a screenshot + console log, then quits.

---

# Vibration Lab Suite (physics simulator)

A 2-D dynamics and vibrations suite built with NOVA: draw a system (balls, blocks, beams, floor lines, springs, dampers, hinges with motors,
pin / roller / fixed supports), run it for as long as you like, then export the telemetry to Excel with charts.

## Where is the .exe?

Download or clone the repo, then double-click:

```
vlab-suite/dist/VibrationLabSuite/VibrationLabSuite.exe
```

It is a single self-contained Windows program (no installer). The same folder also holds `Examples/`, `Documents/` (user guide PDF,
validation report) and `Tools/` (`vlab_cli.exe`, `vlab_tests.exe`). A zip of the whole folder is at
`vlab-suite/dist/VibrationLabSuite-1.1-win64.zip`. Windows SmartScreen may warn on first launch (the exe is unsigned): choose More info, then Run anyway.
Your designs, Excel exports and videos are saved in `Documents\Vibration Lab Suite\`.

## Rebuilding it

Needs MSYS2 mingw64 (g++, cmake, ninja) on PATH: `python vlab-suite/tools/package.py` builds, runs the 51-check validation suite and repackages.
User guide: `docs/Vibration Lab Suite - User Guide.pdf`. Engine guide: `docs/Inside the Vibration Lab - Team NOVA.pdf`.
