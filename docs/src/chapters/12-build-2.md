# Build Guide, Part 2: Editor, Files, Build and Problems

## Step 5: Bring in the code editor

**Goal.** A real code editor appears with colours, line numbers and a minimap.

**Business rules.**

1. Writing code must feel as good as in VS Code: instant response, smart brackets, undo that works.
2. Dark and light themes must both look good.
3. The editor must use the user's font and size settings.

### Loading Monaco

Monaco is delivered as an old-style module system called **AMD**, started by a file named `loader.js`. In `index.html` you add:

```html
<script src="../node_modules/monaco-editor/min/vs/loader.js"></script>
```

Then, in JavaScript (this is NOVA's real `boot` function, trimmed):

```js
async function boot() {
  hydrateIcons();
  S.settings = { ...DEFAULTS, ...(await nova.loadSettings()) };
  applyTheme();
  S.tools = await nova.detectTools();
  updateToolchain();

  const b = new URL('../node_modules/monaco-editor/min/', location.href).href;
  window.MonacoEnvironment = {
    getWorkerUrl: () => 'data:text/javascript;charset=utf-8,' + encodeURIComponent(
      `self.MonacoEnvironment={baseUrl:'${b}'};importScripts('${b}vs/base/worker/workerMain.js');`)
  };
  require.config({ paths: { vs: b + 'vs' } });
  require(['vs/editor/editor.main'], () => { initMonaco(); wireUI(); renderRecent(); renderWelcomeState(); });
}
```

Walk through it:

* `{ ...DEFAULTS, ...(await nova.loadSettings()) }` merges two objects. Saved settings win; anything missing falls back to the defaults. (The spread operator from Chapter 8.)
* `new URL('../node_modules/...', location.href).href` builds the **full address** of the Monaco folder, relative to where `index.html` lives.
* `MonacoEnvironment.getWorkerUrl` tells Monaco where its **workers** are. A **web worker** is a background thread. Monaco uses workers so that heavy jobs (like checking JSON) never freeze typing.
* `require(['vs/editor/editor.main'], callback)` loads Monaco, then runs our `callback` when it is ready.

::: why Why the strange data: URL for the worker?
Workers must normally be loaded from a normal web address, but our page is loaded from a **file** (`file:///...`), where browsers refuse to start a worker straight from another file. The trick: create a tiny script *as text* inside a `data:` URL that simply says "import the real worker file". The browser accepts that. It is a known workaround and the single most puzzling line in the whole project. Without it the editor still appears, but silently loses features and the console fills with worker errors.
:::

::: whatif What if initMonaco ran before Monaco finished loading?
`monaco` would be `undefined` and the first line that used it would crash with *"monaco is not defined"*. That is why everything that needs Monaco is inside the callback. This is the same lesson as the `async/await` section: **wait for the thing you depend on**.
:::

### Creating the editor

```js
S.editor = monaco.editor.create($('#editor'), {
  model: null, theme: 'nova-dark', automaticLayout: true, glyphMargin: true,
  fontSize: S.settings.fontSize, fontFamily: S.settings.fontFamily, fontLigatures: S.settings.ligatures,
  minimap: { enabled: S.settings.minimap, renderCharacters: false },
  smoothScrolling: true, cursorSmoothCaretAnimation: 'on', cursorBlinking: 'smooth',
  bracketPairColorization: { enabled: true }, guides: { bracketPairs: true },
  renderLineHighlight: 'all', padding: { top: 8 }, scrollBeyondLastLine: false,
  mouseWheelZoom: true, fixedOverflowWidgets: true, tabSize: 4, insertSpaces: true,
  stickyScroll: { enabled: true }
});
```

Key options explained:

| Option | Why it is there |
|---|---|
| `model: null` | Start with no file; a *model* is the text of one file |
| `automaticLayout: true` | The editor resizes itself when its container changes (the splitters!) |
| `glyphMargin: true` | Leaves a column for breakpoint dots and the debugger arrow |
| `bracketPairColorization` | Matching brackets share a colour; nesting mistakes are visible |
| `stickyScroll` | The current function's name stays pinned at the top while scrolling |
| `fixedOverflowWidgets` | Popups (completion, hover) can escape the editor box instead of being clipped |

### Our own colours

A **theme** maps *token names* to colours. Monaco's C++ grammar already labels each word (`keyword`, `string`, `number`, `comment`...). NOVA only decides the colours:

```js
monaco.editor.defineTheme('nova-dark', {
  base: 'vs-dark', inherit: true,
  colors: { 'editor.background': '#1e1f22', 'editor.foreground': '#dfe1e5', /* … */ },
  rules: [
    { token: 'comment', foreground: '7a7e85', fontStyle: 'italic' },
    { token: 'keyword', foreground: 'cf8e6d' },
    { token: 'string',  foreground: '6aab73' },
    { token: 'number',  foreground: '2aacb8' },
    { token: 'metatag', foreground: 'b3ae60' }   // #include lines
  ]
});
```

::: why Why "inherit: true"?
It means "start from Monaco's ready-made dark theme and change only what I list". Without it, every colour you forgot would turn black and the editor would be unreadable. Always inherit and override.
:::

### Snippets

A **snippet** is a template that expands when you pick it from the completion list. NOVA registers them for C and C++:

```js
const snip = (label, body, doc) => ({ label, insertText: body, kind: monaco.languages.CompletionItemKind.Snippet,
  insertTextRules: monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet, documentation: doc });
snip('for', 'for (int ${1:i} = 0; ${1:i} < ${2:n}; ++${1:i}) {\n\t$0\n}', 'for loop')
```

`${1:i}` is a **tab stop**: your cursor lands there first, you type a new name, and every other `${1:i}` changes with it. `$0` is where the cursor ends.

::: check Checkpoint 5
Open the DevTools console and run `S.editor.setModel(monaco.editor.createModel('int main() { return 0; }', 'cpp'))`. A coloured C++ line appears with line numbers, a minimap and bracket colours. Press Ctrl+Space after typing `for` and choose the `for` snippet. The cursor should jump between tab stops.
:::

## Step 6: The file tree, tabs and saving

**Goal.** Open a folder, see its files, click to open them in tabs, edit, and save.

### What the main process offers

The renderer cannot touch the disk, so `main.js` offers a small set of **handlers** (you studied the pattern in Chapter 9):

| Channel | Does |
|---|---|
| `fs:readDir` | List a folder (folders first, sorted) |
| `fs:read` | Read a text file (binary files are refused) |
| `fs:write` | Save text |
| `fs:create` | Make a file or folder |
| `fs:rename`, `fs:delete` | Rename or delete |
| `dialog:openFolder` | The system folder picker |
| `fs:listAll` | Every file under a folder (used by Search Everywhere) |

::: why Why such a short, boring list?
Because it is the **entire power** the window has over your disk. If something goes wrong in the window, the damage is limited to those operations. Short lists are easy to audit. (This is called the *principle of least privilege*.)
:::

### The tree, loaded lazily

The tree must feel instant even for a project with 20,000 files. NOVA only reads a folder **when you open it**:

```js
const setOpen = async (open) => {
  row.classList.toggle('open', open); kids.hidden = !open;
  if (open) S.expanded.add(ent.path); else S.expanded.delete(ent.path);
  if (open && !loaded) {
    loaded = true;
    try { for (const c of await nova.readDir(ent.path)) kids.appendChild(await nodeEl(c, depth + 1)); } catch { }
  }
};
```

* `loaded` makes sure each folder is read **once**.
* `S.expanded` is a `Set` remembering which folders are open, so after a refresh the tree reopens the same folders.
* `nodeEl` calls itself for each child: a **recursive** function that draws the whole tree.

::: whatif What if the tree read every folder at start-up?
Opening a project with `node_modules` (often tens of thousands of files) would take many seconds and use hundreds of megabytes. Lazy loading plus the ignore list in `main.js` (`.git`, `node_modules`, `.idea`...) keeps it instant.
:::

### Tabs and models

When you click a file, NOVA does the steps in this figure:

{{fig:tab-lifecycle|From a click in the tree to a dirty dot on the tab.}}

```js
async function openFile(path, pos) {
  let tab = S.tabs.find(t => key(t.path) === key(path));
  if (!tab) {
    let r; try { r = await nova.read(path); } catch (e) { return toast('Cannot open: ' + e.message, 'error'); }
    if (r.binary) return toast('Binary file – not opened in the editor.');
    const model = monaco.editor.createModel(r.text, langOf(path), monaco.Uri.file(path));
    tab = { path, name: base(path), model, dirty: false, saved: model.getAlternativeVersionId(), view: null };
    model.onDidChangeContent(() => {
      const d = model.getAlternativeVersionId() !== tab.saved;
      if (d !== tab.dirty) { tab.dirty = d; renderTabs(); }
    });
    S.tabs.push(tab);
  }
  activate(tab);
}
```

The business rules are visible in the code: *opening a file that is already open just switches to it*; *binary files are refused politely*; *a broken file shows a message instead of crashing*.

::: why Why getAlternativeVersionId and not a simple "changed" flag?
Every edit gives the model a new version number, and **undo** goes back to an *old* number. By storing the version at save time and comparing, NOVA is correct even when you type a letter and then undo it: the numbers match again and the dirty dot disappears. A simple flag would stay "dirty" forever after the first keystroke. It is the same trick editors like VS Code use.
:::

### Remembering where you were

```js
function activate(tab) {
  if (S.active && S.active !== tab) S.active.view = S.editor.saveViewState();
  S.active = tab; S.editor.setModel(tab.model);
  if (tab.view) S.editor.restoreViewState(tab.view);
  renderTabs(); renderCrumbs(); renderWelcomeState(); /* … */
}
```

`saveViewState` stores the scroll position, cursor and selection of the tab you are leaving; `restoreViewState` puts them back. When you switch tabs you return to *exactly* where you left.

### Saving

```js
async function saveTab(tab) {
  await nova.write(tab.path, tab.model.getValue());
  tab.saved = tab.model.getAlternativeVersionId(); tab.dirty = false; renderTabs();
  if (base(tab.path) === 'CMakeLists.txt') { S.cmakeDirty = true; scanTargets(); }
}
```

Notice the **side effect rule**: saving a `CMakeLists.txt` marks the CMake project "dirty" so the next build reconfigures, and rescans for new targets. NOVA also **auto-saves** all changed files when the window loses focus and before every build, so you never compile stale code.

::: why Why auto-save before build?
A classic beginner trap: you edit, press Run, and see the *old* behaviour because you forgot to save. By saving automatically before building, the rule "what you see is what is built" is always true. (A setting turns it off for people who dislike it.)
:::

### Context menu, rename, delete

Right-click a node and `showMenu` builds a small menu from a list of `{ label, run }` objects:

```js
showMenu(e.clientX, e.clientY, [
  { label: 'New File…', run: () => newEntry(parent, false) },
  { label: 'Rename…',   key: 'F2', run: () => renameEntry(ent) },
  { label: 'Reveal in Explorer', run: () => nova.reveal(ent.path) },
  '-',
  { label: 'Delete', danger: true, run: () => deleteEntry(ent) }
]);
```

Deleting first asks for confirmation (`choiceModal`), because it cannot be undone. Renaming also renames any open tab so the file stays linked.

{{img:shot-contextmenu.png|The right-click menu on a file in the tree.}}

::: check Checkpoint 6
Click *Open Folder* and choose a project. The tree shows folders first, then files with coloured icons. Click a file: it opens in a tab. Type a letter: a dot appears on the tab. Press **Ctrl+S**: the dot disappears. Undo your typing before saving: the dot vanishes. Right-click a file: the menu appears. Close the tab with the middle mouse button.
:::

## Step 7: Build and run

**Goal.** One click compiles the code and runs the program; its output appears in a panel; a Stop button works.

**Business rules.**

1. Pressing Run when nothing is ready must say *why*, not fail silently.
2. If the build fails, **do not run** the old program.
3. The user may press Stop at any time.
4. Output must appear live, not all at once at the end.
5. The user can type input to a running program.

### Finding the tools

`main.js` asks the system where each tool is:

```js
function which(cmd) {
  const r = spawnSync(process.platform === 'win32' ? 'where' : 'which', [cmd], { encoding: 'utf8' });
  return r.status === 0 ? r.stdout.split(/\r?\n/)[0].trim() : null;
}
ipcMain.handle('tools:detect', () => {
  const tools = {};
  for (const t of ['g++', 'gcc', 'clang++', 'clang', 'cmake', 'ninja', 'make', 'mingw32-make', 'gdb', 'lldb', 'clang-format', 'clangd', 'git']) {
    const p = which(t); if (p) tools[t] = { path: p, version: ver(t) };
  }
  return tools;
});
```

The window gets an object such as `{ 'g++': {path, version}, cmake: {...} }` and shows "g++ 15.2.0 · CMake 4.2.1" in the status bar. Later code asks `if (!S.tools.cmake)` and shows a clear message.

### Starting a process and streaming its output

In the main process (compare with the Chapter 9 runner):

```js
function startProc(cmd, args, cwd, opts = {}) {
  const id = ++procSeq;
  const child = spawn(cmd, args, { cwd, shell: !!opts.shell, windowsHide: true,
                                   env: { ...process.env, ...(opts.env || {}) } });
  procs.set(id, child);
  const send = (stream, data) => win && !win.isDestroyed() &&
        win.webContents.send('proc:data', { id, stream, data: data.toString() });
  child.stdout.on('data', d => send('out', d));
  child.stderr.on('data', d => send('err', d));
  child.on('error', e => { send('err', `Failed to start: ${e.message}\n`);
                           win.webContents.send('proc:exit', { id, code: -1 }); procs.delete(id); });
  child.on('close', code => { win.webContents.send('proc:exit', { id, code }); procs.delete(id); });
  return { id, child };
}
```

Every process gets an **id** and lives in a `Map`, so many processes (a build, a program, git) can run at once and the window can tell their output apart.

Stopping is more careful than you might expect:

```js
ipcMain.handle('proc:kill', (_e, id) => {
  const p = procs.get(id); if (!p) return;
  if (process.platform === 'win32') spawnSync('taskkill', ['/pid', String(p.pid), '/T', '/F']);
  else p.kill('SIGKILL');
});
```

::: why Why taskkill with /T?
On Windows `child.kill()` ends only the *one* process you started. A build tool starts a compiler which starts other programs; killing only the top one leaves orphans running invisibly. `taskkill /T` ends the whole **tree**, and `/F` forces it. Without this, pressing Stop would leave a program running in the background, still holding your files open.
:::

### The renderer's side: a race condition and its cure

The window starts a process, gets an `id`, and *then* registers a listener for that id. But output could arrive in the tiny gap between. NOVA solves it with a holding area:

```js
const sinks = {}, early = {};
nova.onData(m => { const s = sinks[m.id]; if (s) s.data(m); else (early[m.id] = early[m.id] || []).push({ t: 'd', m }); });
nova.onExit(m => { const s = sinks[m.id]; if (s) s.exit(m.code); else (early[m.id] = early[m.id] || []).push({ t: 'x', m }); });

async function spawnProc(cmd, args, cwd, sink, opts) {
  const id = await nova.start(cmd, args, cwd, opts);
  const wrapped = { data: sink.data, exit: (code) => { delete sinks[id]; sink.exit(code); } };
  sinks[id] = wrapped;
  const pend = early[id] || []; delete early[id];
  for (const ev of pend) ev.t === 'd' ? wrapped.data(ev.m) : wrapped.exit(ev.m.code);
  return id;
}
```

::: whatif What if early output were just dropped?
A program that prints and exits instantly (like "Hello, NOVA!") could finish *before* the window registered its listener. Its output and exit code would vanish, and the Run panel would stay empty. This kind of bug appears only sometimes, which makes it extremely hard to find. A **race condition** is two things racing to happen first. The `early` buffer removes the race.
:::

### The pipeline: build, check, run

```js
async function runPipeline(steps, label) {
  if (S.running) { await stopRun(); }
  $('#runOut').innerHTML = ''; S.problems = []; forceBottom('run'); setBusy(true, label);
  const t0 = performance.now(); let code = 0;
  for (const st of steps) {
    code = await execStep(st, { parseProblems: !st.run });
    if (code !== 0) break;                  // stop at the first failure
  }
  const secs = ((performance.now() - t0) / 1000).toFixed(1);
  out('#runOut', `\nProcess finished with exit code ${code}  (${secs}s)\n`, code === 0 ? 'ok' : 'err');
  setBusy(false, code === 0 ? 'Ready' : `Failed (exit ${code})`);
  renderProblems(); applyMarkers();
  return code;
}
```

`steps` is a list such as `[ buildStep, runStep ]`. The loop is the heart of the business rule *"if the build fails, do not run"*: the `break` on a non-zero exit code means the run step never starts.

### Single file: g++ directly

```js
async function planFile() {
  const tab = S.active;
  const cc = compilerFor(tab.path);
  const outDir = join(S.root || dirname(tab.path), '.nova', 'out');
  const name = base(tab.path).replace(/\.[^.]+$/, ''), exe = join(outDir, name + (IS_WIN ? '.exe' : ''));
  const std = ext(tab.path) === 'c' ? '-std=' + S.settings.cStd : '-std=' + S.settings.cppStd;
  return {
    build: { cmd: cc, args: ['-g', '-O0', std, '-Wall', '-Wextra', ...(await unbufFlag()), tab.path, '-o', exe], cwd: dirname(tab.path) },
    run:   { cmd: exe, args: [], cwd: dirname(tab.path), run: true }
  };
}
```

The flags mean: `-g` keep debug information, `-O0` do not optimise (so stepping follows your lines), `-std=c++17` the language version, `-Wall -Wextra` helpful warnings.

### CMake projects

```js
function cmakeSteps(target) {
  const dir = join(S.root, 'cmake-build-debug'), steps = [];
  if (S.cmakeDirty || !S.cmakeCfgOk) {
    const a = ['-S', S.root, '-B', dir, '-DCMAKE_BUILD_TYPE=Debug', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'];
    if (generator()) a.push('-G', generator());
    if (S.tools.gcc) a.push('-DCMAKE_C_COMPILER=gcc');
    if (S.tools['g++']) a.push('-DCMAKE_CXX_COMPILER=g++');
    steps.push({ cmd: 'cmake', args: a, cwd: S.root, cfg: true });
  }
  const b = ['--build', dir]; if (target) b.push('--target', target);
  b.push('-j', String(navigator.hardwareConcurrency || 4));
  steps.push({ cmd: 'cmake', args: b, cwd: S.root });
  return { dir, steps };
}
```

* **Configure only when needed.** If nothing in `CMakeLists.txt` changed, skip straight to the build; that makes the second Run much faster.
* The compilers are named explicitly so CMake does not pick a different one (such as Visual Studio's) that you did not mean.
* `-j N` builds N files in parallel, using all your processor cores.

To fill the drop-down of run targets, `scanTargets` reads every `CMakeLists.txt` and pulls out the names with a regular expression:

```js
for (const m of txt.matchAll(/add_executable\s*\(\s*([^\s)]+)/gi)) { /* collect m[1] */ }
```

### Programs that read input

The Run panel has a text box at the bottom. When you press Enter, its text plus a newline is written to the program's **stdin**:

```js
$('#runIn').onkeydown = (e) => {
  if (e.key === 'Enter' && S.running) {
    const v = e.target.value; nova.stdin(S.running, v + '\n');
    out('#runOut', v + '\n', 'cmd'); e.target.value = '';
  }
};
```

And the quiet fix for hidden prompts (Chapter 13): NOVA writes `nova_unbuf.h` and adds `-include` of it to every build, so `printf("Enter: ")` shows up before the program waits for you.

{{img:shot-run.png|CHECKPOINT 7: a CMake project built and run. The Run panel shows the two build steps, the exit code with timing, the program's output "Total area: 73.062", and the input box at the bottom.}}

::: check Checkpoint 7
Open (or create) a project with a `CMakeLists.txt` and a `main.cpp` that prints something. The toolbar drop-down shows your target name. Press the green button or **Shift+F10**. The bottom panel opens and shows the `cmake` commands, `[1/2] Building CXX object ...`, `[2/2] Linking CXX executable ...`, your program's output and *"Process finished with exit code 0"*. Press it again: the configure step is skipped and it is faster. Add `while(true){}` to your program and press the red Stop button: it ends.
:::

## Step 8: Problems, squiggles and links

**Goal.** Compiler errors become wavy underlines, a list in the Problems panel, and clickable links.

**Business rules.**

1. Show errors where the user's eyes already are: **on the code**.
2. Clicking an error jumps to its place.
3. Errors from different sources (compiler, language server) appear together, **without duplicates**.
4. A clean build clears the list.

### Parsing the compiler's text

You met the pattern in Chapter 8. Here it is in context:

```js
const DIAG_RE = /^((?:[A-Za-z]:)?[^:\n]+?):(\d+):(?:(\d+):)? (fatal error|error|warning|note): (.*)$/;

function parseDiag(line, cwd) {
  const m = line.match(DIAG_RE); if (!m) return;
  S.problems.push({ file: resolveFile(m[1], cwd), line: +m[2], col: +(m[3] || 1),
                    sev: m[4] === 'note' ? 'note' : m[4] === 'warning' ? 'warning' : 'error', msg: m[5] });
}
```

`resolveFile` turns the compiler's (often relative) path into a full one, because g++ prints paths relative to where it ran.

### Squiggles

```js
function applyMarkers() {
  for (const t of S.tabs) {
    const ms = S.problems.filter(p => key(p.file) === key(t.path)).map(p => {
      const w = t.model.getWordAtPosition({ lineNumber: Math.min(p.line, t.model.getLineCount()), column: p.col });
      const ln = Math.min(p.line, t.model.getLineCount());
      return { startLineNumber: ln, startColumn: p.col, endLineNumber: ln, endColumn: w ? w.endColumn : p.col + 1,
               message: p.msg, severity: p.sev === 'error' ? monaco.MarkerSeverity.Error : /* … */ monaco.MarkerSeverity.Warning,
               source: 'compiler' };
    });
    monaco.editor.setModelMarkers(t.model, 'compiler', ms);
  }
}
```

A **marker** is Monaco's word for "draw a squiggle here with this message". We take the compiler's line and column, find the **word** at that spot (so the squiggle covers `cnt`, not just one letter), and hand the list to Monaco under the owner name `'compiler'`.

::: why Why an "owner" name like 'compiler'?
Markers are grouped by owner. Later, clangd's diagnostics use the owner `'clangd'`. Setting one owner's markers never erases the other's. If both used the same owner, each update would wipe the other's squiggles.
:::

### The Problems panel, merged and de-duplicated

```js
const seen = new Set(), all = [];
for (const p of S.problems.concat(lspProblems())) {
  const k = key(p.file) + ':' + p.line + ':' + p.msg;
  if (!seen.has(k)) { seen.add(k); all.push(p); }
}
```

A `Set` remembers each *file:line:message* already shown. Identical messages from the compiler and clangd appear once. The list is then **grouped by file** and sorted with errors first.

::: whatif What if grouping used the raw file string?
This bug really happened. The compiler wrote the path as `src\bad.cpp` while clangd wrote `c:/.../src/bad.cpp`: the same file with different spellings. The panel split one file into two groups. The fix: group by `key(p.file)`, which lowercases and unifies slashes on Windows. The lesson: **normalise before you compare**.
:::

{{img:shot-problems.png|CHECKPOINT 8: errors in action. Wavy red lines under the faulty code, a red badge on the Problems icon, a yellow warning on line 4, and four entries in the panel. Clicking one jumps to it.}}

::: check Checkpoint 8
Create `bad.cpp` with a missing semicolon and a misspelled variable. Press Run. The build fails, the program does **not** start, red wavy lines appear under the mistakes, a red badge shows on the Problems icon, and the list shows each message with its line and column. Click one: the cursor jumps there. Fix the file and run again: the squiggles and the list clear.
:::

## Mini assignments

::: assign Assignment 15.1: Ignore list
Add `.vscode` and `build` to the ignore list in `main.js`. Reload and confirm those folders no longer appear in the tree.
:::

::: assign Assignment 15.2: Status counts
Show "Ln 12, Col 5, 3 lines selected" in the status bar by listening to `S.editor.onDidChangeCursorSelection`.
:::

::: assign Assignment 15.3: Build timer
Make the status bar show the build time ("Built in 2.4s") after each successful build, and keep it until the next action.
:::

::: assign Assignment 15.4: A clean-build button
Add *Clean* to the Actions list. It should delete the `cmake-build-debug` folder (with confirmation) and mark the project dirty so the next Run reconfigures.
:::

## Quiz

::: quiz
1. What is a Monaco *model*?
2. Why does NOVA compare version ids instead of using a "changed" flag?
3. Why is the file tree loaded lazily?
4. Why does Stop use `taskkill /T`?
5. What is a race condition? Which one did `spawnProc` have to solve?
6. Why does `runPipeline` stop at the first non-zero exit code?
7. Why does NOVA skip the CMake configure step on the second Run?
8. Why do problem markers use an owner name?
9. Why must paths be normalised before being compared?
:::

::: answers
1. The text (and its undo history) of one file.
2. Undoing back to the saved text should clear the dirty state; version ids make that automatic.
3. Reading every folder at start-up would be slow for big projects; reading on demand keeps the UI instant.
4. Windows only kills the one process by default; `/T` kills the whole tree of child processes so nothing is left running.
5. Two things racing to happen first. Output or the exit event could arrive before the window registered its listener; the `early` buffer replays them.
6. Because a failed build must not run the old or half-built program.
7. Nothing in CMakeLists.txt changed, so the configuration is still valid; this makes builds faster.
8. So that different sources (compiler, clangd) can set and clear their own markers without wiping each other's.
9. The same file can be spelled differently (slashes, upper/lower case); without normalising, equal paths look different.
:::
