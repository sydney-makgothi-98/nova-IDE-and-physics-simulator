# Build Guide, Part 3: Search, Terminal, Intelligence and Debugger

## Step 9: Search, Search Everywhere and the command palette

**Goal.** Find text across the whole project, jump to any file or symbol by typing a few letters, and run any action by name.

**Business rules.**

1. Finding a file must take about **two seconds** no matter how big the project is.
2. Typing "shp" should find `shapes.cpp`: the letters must match **in order, not necessarily next to each other** (this is called *fuzzy matching*).
3. Results that match at the start of a word, or in a row, should rank higher.
4. The shortcut must be fast to reach: **double-tap Shift**, like JetBrains.

### Find in Files (done by the main process)

The window sends the query; the main process walks the folders and reads the files; it sends back a list of matches. Doing the heavy work in the main process means the window never freezes.

```js
ipcMain.handle('fs:grep', (_e, root, query, opts = {}) => {
  const files = []; walk(root, files);
  const results = [];
  const src = opts.regex ? query : query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const re = new RegExp(opts.word ? `\\b${src}\\b` : src, opts.case ? '' : 'i');
  for (const f of files) {
    const buf = fs.readFileSync(f); if (buf.includes(0)) continue;      // skip binaries
    buf.toString('utf8').split(/\r?\n/).forEach((ln, i) => {
      if (re.test(ln) && results.length < 2000) results.push({ file: f, line: i + 1, text: ln.trim().slice(0, 300) });
    });
  }
  return results;
});
```

Three protective details, each a business rule in disguise:

* The long `replace` **escapes** special characters, so searching for `a.b` or `(x)` means the literal text, not a pattern. (Unless the user ticks the regex box.)
* Files over 2 MB and binary files are skipped, so one giant log cannot freeze the search.
* Results stop at 2000. Nobody reads more, and a million-item list would crash the display.

{{img:shot-find.png|CHECKPOINT 9a: Find in Files for "radius". Results are grouped by file with counts; each line is clickable.}}

### Fuzzy matching

This is the real scoring function from NOVA's `app.js`. You can run it.

```js run
function fuzzy(q, s) {
  q = q.toLowerCase(); const t = s.toLowerCase(); let i = 0, score = 0, last = -2;
  for (let j = 0; j < t.length && i < q.length; j++) {
    if (t[j] === q[i]) {
      score += (j === last + 1 ? 5 : 1) + (j === 0 || /[\/\\._\- ]/.test(t[j - 1]) ? 4 : 0);
      last = j; i++;
    }
  }
  return i === q.length ? score - t.length * 0.01 : -1;
}

const files = ["shapes.cpp", "shapes.h", "CMakeLists.txt", "main.cpp", "sh_utils.cpp", "dispatch.cpp"];
for (const q of ["sha", "shp", "mc", "cpp", "zzz"]) {
  const ranked = files.map(f => [f, fuzzy(q, f)]).filter(x => x[1] >= 0).sort((a, b) => b[1] - a[1]).map(x => x[0]);
  console.log(q.padEnd(4), "->", ranked.join(", ") || "(nothing)");
}
```

How it scores:

1. Walk through the name; every time the next wanted letter is found, give a point.
2. **+5 bonus** if it directly follows the previous matched letter (a run like "sha").
3. **+4 bonus** if it begins the name or follows a separator (`/ \ . _ - space`): word starts matter.
4. A tiny **penalty for length**, so shorter names rank above longer ones with the same match.
5. If not all the letters were found, return `-1`: no match.

::: why Why such a simple formula?
Because it runs on **thousands** of names on every keystroke and must feel instant. Real IDEs use cleverer scoring, but this 8-line version captures what users expect: starts of words and unbroken runs win. Simple, fast and good enough is often the right engineering answer.
:::

### Double Shift

```js
let lastShift = 0, shiftClean = false;
addEventListener('keydown', (e) => { if (e.key === 'Shift') { if (!e.repeat) shiftClean = true; } else shiftClean = false; }, true);
addEventListener('keyup', (e) => {
  if (e.key === 'Shift' && shiftClean) {
    const n = Date.now();
    if (n - lastShift < 380) { lastShift = 0; openPalette('all'); } else lastShift = n;
  }
}, true);
```

`shiftClean` is true only if Shift was pressed **alone** (no other key in between), so typing a capital letter with Shift never triggers it. Two clean releases within 380 milliseconds open the palette.

::: whatif What if there were no shiftClean?
Every time you typed "Hello" (Shift+H), the release of Shift would count as a tap, and typing two capital letters fast would pop up the search window in the middle of your sentence. The extra flag prevents that annoyance.
:::

### One list for files, symbols and actions

Search Everywhere merges three sources: **files** (from `fs:listAll`, cached), **symbols** (from the Structure panel) and **actions** (a plain array of `{ name, key, run }`). Pressing Tab cycles *All, Files, Symbols, Actions*. Pressing Enter runs the highlighted item. The action list is how you reach everything without a mouse, for example *Restart Language Server (clangd)*.

::: check Checkpoint 9
Press **Shift twice**. A window appears. Type `sha`: `shapes.cpp` and `shapes.h` are first. Press Enter: the file opens. Press **Ctrl+Shift+F**, type a word, press Enter: matches are listed by file and clicking one jumps to it. Press **Ctrl+Shift+A** and type `term`: the terminal action appears.
:::

## Step 10: A real terminal

**Goal.** A genuine command line inside NOVA.

**Business rules.**

1. Everything must behave as in a normal terminal: colours, history, Ctrl+C, programs that ask questions.
2. It starts in the **project folder**.
3. When the shell exits, the user can restart it with a keypress.
4. It matches the IDE theme.

### The shell behind the glass

In the main process:

```js
ipcMain.handle('pty:spawn', (_e, cols, rows, cwd) => {
  const shell = process.platform === 'win32' ? 'powershell.exe' : (process.env.SHELL || 'bash');
  const env = { ...process.env }; delete env.ELECTRON_RUN_AS_NODE;
  const id = ++ptySeq;
  const t = nodePty.spawn(shell, ['-NoLogo'], { name: 'xterm-256color', cols: cols || 80, rows: rows || 24, cwd: cwd || os.homedir(), env });
  ptys.set(id, t);
  t.onData(d => win.webContents.send('pty:data', { id, data: d }));
  t.onExit(() => { ptys.delete(id); win.webContents.send('pty:exit', { id }); });
  return { id };
});
```

`node-pty` starts the shell attached to a **pseudo-terminal**, so the shell believes a real person sits at a real terminal.

### The face: xterm.js

In the window, `terminal.js` creates the drawing surface and wires the two ends together:

```js
T.term = new Terminal({ fontFamily: S.settings.fontFamily, fontSize: Math.max(11, S.settings.fontSize - 1),
                        cursorBlink: true, theme: termTheme(), scrollback: 5000 });
T.fit = new FitAddon.FitAddon(); T.term.loadAddon(T.fit); T.term.open(host); T.fit.fit();
T.term.onData(d => T.id && nova.ptyWrite(T.id, d));          // keys you press  -> shell
T.term.onResize(({ cols, rows }) => T.id && nova.ptyResize(T.id, cols, rows));
nova.onPtyData(({ id, data }) => { if (id === T.id && T.term) T.term.write(data); });   // shell output -> screen
```

Three pipes in a loop: **keyboard → xterm → IPC → PTY → shell**, and back **shell → PTY → IPC → xterm → screen**.

::: why Why the "fit" add-on and the resize message?
The shell must know how many columns and rows the window has, so that long lines wrap in the right place and programs such as `nano` or `top` draw correctly. When you drag the splitter, the add-on measures the new size and NOVA tells the PTY. Without it, a resized terminal would wrap text in the wrong place and full-screen programs would look garbled.
:::

::: whatif What if the terminal started lazily or not at all?
NOVA starts the shell the **first time** you open the Terminal tab (`termEnsure`). If it started at launch, every startup would pay for a PowerShell process nobody may use. Lazy start makes the IDE open faster.
:::

{{img:shot-terminal.png|CHECKPOINT 10: the integrated terminal running two commands. This is a real PowerShell, with its own prompt, started in the project folder.}}

::: check Checkpoint 10
Press **Alt+F12**. A prompt appears in your project folder. Type `g++ --version` and `dir`. Type `exit`: the message "process exited, press any key to restart" appears; press a key and a fresh shell starts. Drag the splitter above the panel: the text re-wraps. Switch the theme in Settings: the terminal changes colours.
:::

## Step 11: Code intelligence with clangd

**Goal.** Completion, hover documentation, go to definition, rename, quick fixes and live error underlines, from the real C++ compiler front-end.

**Business rules.**

1. clangd must understand the project **exactly** as the real compiler does.
2. The editor must never freeze waiting for it.
3. If clangd is missing or crashes, the editor still works.
4. The server and editor must always agree on the text of every open file.

### Giving clangd the facts

```js
async function lspPrepareCompileDb() {
  if (S.isCmake) {
    const dir = join(S.root, 'cmake-build-debug');
    if (await nova.exists(join(dir, 'compile_commands.json'))) return dir;
    if (S.tools.cmake) {                       // silent configure so clangd gets real flags
      setLspStatus('configuring CMake…');
      /* … runs the cmake configure step …*/
      if (await nova.exists(join(dir, 'compile_commands.json'))) return dir;
    }
  }
  /* fallback: write our own compile_commands.json for loose files into .nova/ */
}
```

Order of preference: (1) CMake's real database; (2) run CMake quietly to make one; (3) invent one for loose files. In every case clangd gets *some* answer.

### Starting the server

```js
ipcMain.handle('lsp:start', (_e, root, compileDir) => {
  const exe = which('clangd'); if (!exe) return { error: 'clangd not found' };
  const args = ['--background-index', '--clang-tidy=false', '--header-insertion=never',
                '--completion-style=detailed', '--query-driver=**/g++*,**/gcc*,**/clang*', '--log=error'];
  if (compileDir) args.push('--compile-commands-dir=' + compileDir);
  const child = spawn(exe, args, { cwd: root, windowsHide: true });
  /* …read framed messages from child.stdout and forward them to the window… */
});
```

| Option | Why |
|---|---|
| `--background-index` | Index the whole project quietly so "find references" and "go to definition" work across files |
| `--header-insertion=never` | Do not edit the user's `#include` lines behind their back |
| `--completion-style=detailed` | Show full signatures in the list |
| `--query-driver=...` | Ask **your** g++ where its standard headers are, so `<vector>` is found |
| `--compile-commands-dir` | Where to find `compile_commands.json` |

::: why Why --query-driver?
clangd is built on LLVM but your project uses MinGW's g++ and its own copy of the standard library. Without this option clangd cannot find `<iostream>` and underlines every standard include in red. It is a perfect example of *small flags with huge effects*.
:::

### Talking in requests

The JSON-RPC client is tiny:

```js
function lspRequest(method, params, timeout = 8000) {
  if (!LSP.on) return Promise.resolve(null);
  return new Promise((res) => {
    const id = ++LSP.id;
    const t = setTimeout(() => { LSP.pending.delete(id); res(null); }, timeout);
    LSP.pending.set(id, (r) => { clearTimeout(t); res(r); });
    lspSend({ id, method, params });
  });
}
```

It is the `request`/`pending` code from Chapter 13, with one extra line: if clangd is not running (`LSP.on` false) the answer is instantly `null`, so the editor carries on as normal. That is rule 3 in one line.

### Keeping the server in sync

```js
function lspDidChange(tab) {
  if (!LSP.initialized || !isCFamily(tab) || !LSP.versions.has(tab.path)) return;
  clearTimeout(LSP.timers.get(tab.path));
  LSP.timers.set(tab.path, setTimeout(() => lspFlush(tab), 120));       // debounce
}
```

The 120 ms debounce (Chapter 8) means a burst of typing causes **one** message. And before any request such as completion, NOVA calls `lspFlush(tab)` to make sure the server has the very latest text. Rule 4 in practice.

### Plugging into Monaco

Monaco has "provider" hooks. You register a function for each feature; Monaco calls it at the right moment. Here is hover:

```js
monaco.languages.registerHoverProvider(lang, {
  async provideHover(model, pos) {
    const t = live(model); if (!t) return null; sync(t);
    const r = await lspRequest('textDocument/hover', { textDocument: { uri: lspUri(t.path) }, position: toPos(pos) });
    if (!r || !r.contents) return null;
    const c = r.contents; const arr = Array.isArray(c) ? c : [c];
    return { range: r.range ? fromRange(r.range) : undefined,
             contents: arr.map(x => typeof x === 'string' ? { value: x } : { value: x.value }) };
  }
});
```

Everything that the user sees comes from that tiny bridge: **Monaco asks → NOVA translates positions → clangd thinks → NOVA translates the reply → Monaco draws.** The same shape repeats for completion, signature help, definition, references, outline, formatting, rename and quick fixes (all in `lsp.js`).

{{img:shot-hover.png|CHECKPOINT 11a: hover. Pointing at a variable shows clangd's description: its kind, its type and where it was declared.}}

::: whatif What if the diagnostics used the compiler only?
You would see errors only when you press Run. With clangd, a squiggle appears about a second after you stop typing. The compiler is the final judge; clangd is the quick friend that spots slips as you go.
:::

### The outline from the server

The Structure panel used to be built from a regular expression. With clangd it asks the server for *document symbols* and indents children under their classes:

```js
async function lspStructure(tab) {
  const r = await lspRequest('textDocument/documentSymbol', { textDocument: { uri: lspUri(tab.path) } }, 5000);
  if (!r) return null;                       // no answer: fall back to the regex version
  /* …flatten the tree into rows with a depth… */
}
```

{{img:shot-structure.png|CHECKPOINT 11b: the Structure panel, filled by clangd. Clicking a row jumps to that line.}}

::: check Checkpoint 11
Open a C++ project. After a few seconds the status bar says **clangd**. Type `std::vec` and pause: a list appears with `vector`. Hover over a function: documentation appears. **Ctrl+click** a function call: you jump to its definition. Press **F2** on a variable: rename it everywhere. Type a mistake: a red squiggle appears with no build. Open *Actions → Restart Language Server* and see it recover.
:::

## Step 12: The debugger

**Goal.** Stop a running program at breakpoints, look at its variables, and step through it line by line.

**Business rules.**

1. Breakpoints are set by clicking the margin, and can be added or removed **while** the program runs.
2. The debugger builds first (with `-g`); if the build fails it must not start.
3. When the program stops, the editor must show **exactly where**: highlighted line, arrow, and the right file.
4. Buttons must only be enabled when they make sense.
5. Crashes (like a null pointer) must be reported in plain words.

### Starting gdb and talking MI

```js
async function dbgStart(exe, cwd) {
  const r = await nova.gdbStart(exe, cwd, []);
  if (r.error) return toast('Could not start gdb: ' + r.error, 'error');
  Object.assign(D, { on: true, running: false, frames: [], vars: [], sel: 0, bpNum: new Map(), exe, prevVars: new Map() });
  await dbgCmd('-gdb-set breakpoint pending on');
  await dbgCmd('-gdb-set confirm off');
  await dbgCmd('-gdb-set new-console ' + (S.settings.debugExtConsole === false ? 'off' : 'on'));
  await dbgCmd('-environment-cd ' + miq(fwd(cwd)));
  for (const [k, set] of Object.entries(S.breaks)) { /* … */ for (const line of set) await dbgInsert(path, line); }
  dbgCmd('-exec-run');
}
```

Each `await dbgCmd(...)` sends one MI command and waits for its numbered answer (the token matching you saw in Chapter 13). The setup commands are rules in disguise:

* `breakpoint pending on`: a breakpoint in code that is not loaded yet is accepted instead of refused.
* `confirm off`: gdb never stops to ask "are you sure?", which would freeze our machine-driven conversation.
* `new-console on`: your program gets its own window, so it can read the keyboard.

### Reacting to a stop

```js
async function dbgStopped(d) {
  D.running = false;
  const reason = d.reason || '';
  if (reason.startsWith('exited')) { /* print the exit code and end the session */ return; }
  if (reason === 'signal-received' || reason === 'exited-signalled') {
    const msg = `Program received signal ${d['signal-name']} (${d['signal-meaning']})`;
    dcon('\n' + msg + '\n', 'err'); toast(esc(msg), 'error', 6000);
  }
  D.sel = 0; await dbgRefresh(true);
}
```

`dbgRefresh` asks gdb for the stack (`-stack-list-frames`) and variables (`-stack-list-variables --all-values`), draws the panels, opens the file and highlights the line:

```js
function dbgDecorate() {
  if (D.on && !D.running && D.last && S.active && key(S.active.path) === key(D.last.path))
    D.deco.set([{ range: new monaco.Range(D.last.line, 1, D.last.line, 1),
                  options: { isWholeLine: true, className: 'dbg-line', glyphMarginClassName: 'dbg-arrow' } }]);
  else D.deco.set([]);
}
```

A **decoration** is Monaco's word for "style this range of text". The yellow arrow and blue band are just CSS classes (`dbg-arrow`, `dbg-line`) applied to the current line.

### Live breakpoints

```js
function dbgSyncBreak(path, line, added) {
  if (!D.on) return;
  const k = key(path) + ':' + line;
  if (added) dbgInsert(path, line);
  else if (D.bpNum.has(k)) { dbgCmd('-break-delete ' + D.bpNum.get(k)); D.bpNum.delete(k); }
}
```

gdb gives each breakpoint a **number** when it is created; NOVA remembers it in a `Map` keyed by *file:line* so it can delete it later. That is rule 1.

### The buttons obey the state machine

```js
function dbgUi() {
  const on = D.on, stopped = on && !D.running;
  $$('#dbgBar [data-d]').forEach(b => { b.disabled = b.dataset.d === 'stop' ? !on : !stopped; });
  $('#dbgState').textContent = !on ? 'Not debugging' : D.running ? 'Running…' : 'Paused';
}
```

Two booleans, four states (the figure in Chapter 13), and one function that sets every button. Rule 4.

::: why Why does the Variables panel highlight changed values?
You press Step and ask yourself "what did that line do?" NOVA remembers the previous values (`D.prevVars`) and draws any that changed in a warm colour. Your eye jumps straight to the effect of the line you just ran. It is a tiny feature that makes debugging noticeably faster.
:::

::: whatif What if you stepped while the program was running?
gdb would reject the command or behave unpredictably. With buttons disabled during *Running*, the user cannot even try. Preventing mistakes in the interface is better than reporting them afterwards.
:::

{{img:shot-debug.png|CHECKPOINT 12: the debugger paused on line 9. The arrow and band mark the current line; the red dot would be the breakpoint; Frames, Variables, Watches and the gdb console fill the panel. The floating box is the hover evaluation.}}

::: check Checkpoint 12
Open a project and click in the margin next to a line inside a loop (a red dot appears). Press **Shift+F9**. After the build, the program stops there: the line is highlighted with an arrow, Variables show the current values, Frames show the call stack. Press **F8** to step over: values that changed turn orange. Press **F9** to resume: it stops at the next hit. Add `v * 10` as a watch. Hover a variable: its value pops up. Press the red Stop button: the session ends cleanly.
:::

## Finishing touches

### Settings

The Settings window edits the `S.settings` object and saves it (as JSON) to the user's data folder. Changes apply **live**: toggling the theme calls `applyTheme()`, which sets `data-theme`, updates Monaco's theme and the terminal's colours.

{{img:shot-settings.png|The Settings window. Everything changes the moment you pick it.}}

### The new project wizard

It checks the name with a regular expression (letters, digits, `_`, `-`), refuses a folder that already exists, then writes `CMakeLists.txt`, `main.cpp` (or `main.c`) and `.gitignore`, and opens the new project.

{{img:shot-newproject.png|The New Project wizard. Choose C or C++, the standard, and a location.}}

### The dark theme

{{img:shot-dark.png|The default Darcula-style dark theme. All the screenshots in this book use the light theme for printing, but this is how NOVA looks by default.}}

### The test harness

How do you test a window full of buttons without clicking? NOVA has a hidden **self-test mode**. If the environment variable `NOVA_SELFTEST` points to a JavaScript file, NOVA starts, runs that file *inside the window*, saves a screenshot and the console log, and quits:

```js
win.webContents.on('did-finish-load', async () => {
  await new Promise(r => setTimeout(r, 2500));
  const script = fs.readFileSync(process.env.NOVA_SELFTEST, 'utf8');
  const r = await win.webContents.executeJavaScript(script);      // run the test script
  const img = await win.webContents.capturePage();                // take a picture
  fs.writeFileSync(process.env.NOVA_SHOT, img.toPNG());
  app.quit();
});
```

A test script can simply call NOVA's own functions: `await openProject(...)`, `await doRun()`, `toggleBreak(...)`, `await startDebugSession()`, and then return data to check. **Every screenshot in this book was captured this way**, and so were the checks that proved the debugger, language server and terminal worked.

::: why Why build testing into the program?
UI code is notoriously hard to test, so many teams skip it, and bugs creep in. A 15-line harness that can "press the buttons" and take pictures makes it possible to check the whole pipeline in under a minute after every change. It also produced this book's pictures, which would otherwise have had to be taken by hand.
:::

## The file-by-file reference

Keep this table next to you when you read the source code.

### The project root

| File or folder | What it is |
|---|---|
| `package.json` | Name, version, entry point (`main`), start script, libraries |
| `main.js` | The **main process**: window, disk, processes, PTY, clangd, gdb, settings |
| `preload.js` | The safe bridge: defines `window.nova` for the window |
| `launch.js` and `Launch NOVA.bat` | Start Electron without `ELECTRON_RUN_AS_NODE` |
| `README.md` | Short instructions for the team |
| `node_modules/` | Downloaded libraries (never edit; safe to delete and reinstall) |
| `renderer/` | Everything that runs inside the window |
| `docs/` | This book, its Markdown source, diagram generator and build script |

### main.js (about 250 lines), in the order they appear

| Section | What it does |
|---|---|
| `createWindow` | The window options, preload, and loading `index.html` |
| file system handlers | `fs:readDir`, `fs:read`, `fs:write`, `fs:create`, `fs:rename`, `fs:delete`, `fs:reveal`, `fs:listAll`, `fs:grep` |
| dialogs | `dialog:openFolder`, `dialog:openFile` |
| `which`, `tools:detect` | Finds compilers and tools on the PATH |
| `startProc` and `proc:*` | Start, feed (`stdin`) and stop programs; streams output as events |
| PTY handlers | `pty:spawn`, `pty:write`, `pty:resize`, `pty:kill` |
| clangd handlers | `lsp:start`, `lsp:send`, `lsp:stop` with the Content-Length reader |
| gdb handlers | `gdb:start`, `gdb:send`, `gdb:stop`, `gdb:interrupt` |
| `app:unbuf` | Writes the header that un-buffers `printf` |
| settings and window handlers | `settings:load`, `settings:save`, `win:*`, and the self-test hooks |

### renderer files

| File | What it contains |
|---|---|
| `index.html` | The skeleton: title bar, stripe, sidebar, editor area, bottom panes, status bar |
| `styles.css` | Variables for both themes, grid layout, tabs, tree, panels, modals, debugger and terminal styling |
| `icons.js` | The SVG icon table and the `icon()` / `hydrateIcons()` helpers |
| `app.js` | The main logic: utilities (toast, menu, modals), file icons, `boot`, theme, Monaco setup, settings, project open/new, file tree, tabs and saving, breakpoints, structure view, panels and splitters, process running (`runPipeline`, `planFile`, `cmakeSteps`, `scanTargets`), problems and markers, Find in Files, Search Everywhere and actions, and the keyboard shortcuts |
| `lsp.js` | The clangd client: start-up and compile database, message routing, document sync, diagnostics, all the Monaco providers, rename edits and the outline |
| `debug.js` | The gdb client: the MI parser, command/answer matching, session start and stop, stack, variables and watches, stepping controls, decorations and hover evaluation |
| `terminal.js` | xterm.js set-up, PTY connection, resize, restart and theme |

### The shared object `S`

| Field | Meaning |
|---|---|
| `S.root` | The open project folder |
| `S.tabs`, `S.active` | Open files and the one on screen |
| `S.settings` | User settings (also saved to disk) |
| `S.tools` | Which compilers and tools were found |
| `S.config`, `S.targets` | The selected run target and the targets found in CMake |
| `S.problems` | Diagnostics from the compiler for the last build |
| `S.breaks` | Breakpoints by file |
| `S.expanded` | Which folders are open in the tree |
| `S.running` | The id of the program that is currently running |

::: check Final checkpoint
Start NOVA with `Launch NOVA.bat`. You can: create a project; see colours and wavy errors as you type; build and run with one key; read the output; debug with breakpoints, stepping and watches; use the terminal; search by double-tapping Shift; change themes. If all of that works, you have built a real IDE. Congratulations.
:::

## Mini assignments

::: assign Assignment 16.1: A new action
Add an action *Copy File Path* to the Actions list that copies the active tab's path to the clipboard (`navigator.clipboard.writeText`). Show a toast when done.
:::

::: assign Assignment 16.2: Recent files
Remember the last ten opened files in `S.settings` and add a *Recent Files* list to Search Everywhere that shows them first when the search box is empty.
:::

::: assign Assignment 16.3: Colour the problems badge
Make the Problems badge show the number of **errors** in red, or, when there are none, the number of **warnings** in yellow. (The code for this is in `renderProblems`; find it and read it first.)
:::

::: assign Assignment 16.4: A log pane for clangd
Write every LSP message to a hidden array and add a debug action *Show LSP Log* that opens it in a tab. It teaches you how the protocol really behaves.
:::

::: assign Assignment 16.5: Break on condition
Extend breakpoints so that Shift+click makes a **conditional** breakpoint. Use MI's `-break-insert -c "condition"` form.
:::

## Quiz

::: quiz
1. Why is Find in Files done in the main process?
2. In the fuzzy function, which two things earn bonuses?
3. What problem does the `shiftClean` flag prevent?
4. Why does the terminal use a PTY?
5. What does `--query-driver` solve for clangd?
6. Why does NOVA debounce `didChange`, and why also flush before a request?
7. What does `-gdb-set confirm off` do and why is it needed?
8. How does NOVA know which breakpoint number to delete?
9. How is the current debug line drawn in the editor?
10. What is the NOVA self-test mode and what is it good for?
:::

::: answers
1. It reads many files; doing that in the window would freeze the interface. The main process can walk the disk and return only the results.
2. A match directly after the previous matched letter, and a match at the start of a name or after a separator.
3. Capital letters (Shift pressed together with another key) triggering Search Everywhere.
4. Shells and tools only behave interactively (colours, prompts, full-screen programs) when they think they are attached to a terminal.
5. It lets clangd ask your g++ where its standard headers are, so standard includes are found.
6. Debounce sends one message per burst of typing; flushing before a request makes sure the server has the very latest text.
7. It stops gdb from asking "are you sure?" prompts which would block the machine-driven conversation.
8. gdb returns a number on creation; NOVA stores it in a Map keyed by file and line.
9. A Monaco decoration with CSS classes (`dbg-line`, `dbg-arrow`) on the current line.
10. An environment variable runs a script inside the window, saves a screenshot and log, and quits. It allows automatic testing and taking screenshots.
:::
