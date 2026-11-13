@@part Part III — Building NOVA, Step by Step

# Build Guide, Part 1: Setup and the First Window

## The plan

You now know the languages. In this part you will **build NOVA** from an empty folder, in twelve steps. Each step adds one feature, ends with a **CHECKPOINT** that shows what you should see, and explains the **business logic**: what the feature must do for the person using the IDE, and why.

{{fig:progress-ladder|The twelve steps of the build. Each one leaves you with a working program.}}

::: def Business logic
In this book, **business logic** means *the rules about what the program must do for its user*. For example: "A tab shows a dot when its file has unsaved changes" or "When a build fails, show the errors and do not start the program". Business logic is different from *technical* code (how to read a file). Good programmers write the rules first, then the code.
:::

How to use this part:

* Build in order. Each step uses the one before it.
* Type the code yourself. The finished project is in the folder `NOVA - C Interpreter` for comparison.
* After each step, run the program and compare your screen with the checkpoint.
* Where the book shows a short excerpt, the full version is in the matching file. This book cannot print all two thousand lines, but it explains every important one.

## Step 1: Install the tools

**Goal.** Get a computer ready.

**Business rule.** NOVA does not contain a compiler. It drives tools the user already has. So the machine must have them, and NOVA must *find* them.

On **Windows** the easiest way to get a full C and C++ toolchain is **MSYS2** (msys2.org). Install it, open the *MSYS2 MINGW64* terminal, and run:

```text
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gdb mingw-w64-x86_64-clang-tools-extra
```

This installs, in one go: g++ and gcc (compilers), CMake, Ninja (fast build tool), gdb (debugger), and clangd plus clang-format (code intelligence and formatting). Then add `C:\msys64\mingw64\bin` to your Windows **PATH** so that every program can find them.

::: def PATH
**PATH** is a list of folders. When you type `g++`, the system looks through those folders, in order, for a program with that name. If the folder holding g++ is not in PATH you get *"g++ is not recognised"*. NOVA searches PATH the same way (with the `where` command on Windows).
:::

Then install **Node.js** (LTS version) from nodejs.org. It brings `npm`, the package installer.

Open a normal terminal and check everything:

```text
node --version
npm --version
g++ --version
cmake --version
gdb --version
clangd --version
```

::: check Checkpoint 1
Every command prints a version number (yours may be newer than ours: Node 24, g++ 15.2, CMake 4.2, gdb 17). If one says "not recognised", fix your PATH and open a **new** terminal (old terminals do not see the change).
:::

::: why Why check from a terminal first?
If NOVA cannot find g++, the Run button will fail and it will be hard to tell whether the bug is in NOVA or in your setup. Two minutes of checking now saves an hour of confusion later. Professionals call this *eliminating variables*.
:::

## Step 2: Create the project

**Goal.** An empty project with the building blocks downloaded.

Make a folder (we used `NOVA - C Interpreter`). Open a terminal inside it and run:

```text
npm init -y
npm install electron monaco-editor node-pty @xterm/xterm @xterm/addon-fit
```

`npm init -y` creates `package.json`. `npm install` downloads the libraries into a folder called `node_modules` and records them in `package.json`. Edit `package.json` so it looks like this:

```json
{
  "name": "nova-ide",
  "productName": "NOVA IDE",
  "version": "0.1.0",
  "description": "In-house C/C++ IDE inspired by the JetBrains workflow",
  "main": "main.js",
  "scripts": { "start": "node launch.js" },
  "license": "UNLICENSED",
  "private": true,
  "dependencies": {
    "electron": "^45.3.1",
    "monaco-editor": "^0.52.2",
    "node-pty": "^1.1.0",
    "@xterm/addon-fit": "^0.11.0",
    "@xterm/xterm": "^6.0.0"
  }
}
```

(Your version numbers will differ a little. That is fine.)

| Library | Its job in NOVA |
|---|---|
| `electron` | The window, and a built-in Node.js |
| `monaco-editor` | The code editor (the same one VS Code uses) |
| `node-pty` | A pseudo-terminal, for the real terminal panel |
| `@xterm/xterm` and `@xterm/addon-fit` | Draws the terminal in the window and resizes it |

::: why Why "main": "main.js"?
Electron needs to know which file to run first. The `main` field in `package.json` is that file. It becomes the *main process*. If it is missing or misspelled, Electron shows a default welcome window instead, and you will wonder why your code never runs.
:::

::: warn Watch out
If, after `npm install`, `node_modules/electron/dist/electron.exe` is missing, the download step was skipped. Run `node node_modules/electron/install.js` once to fetch it. We hit this on the first install.
:::

::: check Checkpoint 2
You have: `package.json`, `package-lock.json`, and a `node_modules` folder with `electron`, `monaco-editor`, `node-pty` and `@xterm` inside.
:::

## Step 3: The first window

**Goal.** A window opens. Nothing else yet.

**Business rule.** The window should look modern and professional from the first frame: dark, no flash of white, with the native minimise/maximise/close buttons kept.

Create `main.js` (this is a trimmed version of NOVA's real start):

```js
const { app, BrowserWindow } = require('electron');
const path = require('path');

let win;

function createWindow() {
  win = new BrowserWindow({
    width: 1500, height: 920, minWidth: 900, minHeight: 600,
    backgroundColor: '#1e1f22',
    frame: false,
    titleBarStyle: 'hidden',
    titleBarOverlay: { color: '#2b2d30', symbolColor: '#dfe1e5', height: 40 },
    title: 'NOVA IDE',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false
    }
  });
  win.setMenuBarVisibility(false);
  win.loadFile(path.join(__dirname, 'renderer', 'index.html'));
}

app.whenReady().then(createWindow);
app.on('window-all-closed', () => app.quit());
```

Create `preload.js` (empty bridge for now):

```js
const { contextBridge } = require('electron');
contextBridge.exposeInMainWorld('nova', { ping: () => 'pong' });
```

Create `renderer/index.html` with a title and one line of text, then create `launch.js`:

```js
// Starts NOVA IDE. Clears ELECTRON_RUN_AS_NODE, which would make Electron behave like plain Node.
const { spawn } = require('child_process');
const electron = require('electron');
const env = { ...process.env }; delete env.ELECTRON_RUN_AS_NODE;
spawn(electron, [__dirname, ...process.argv.slice(2)], { env, stdio: 'inherit', detached: true }).unref();
```

Run it:

```text
npm start
```

::: why Why a separate launch.js?
Some tools (including editor extensions) leave `ELECTRON_RUN_AS_NODE=1` in the environment. Electron then behaves like plain Node, `require('electron')` returns nothing useful, and the program dies with an unreadable error. `launch.js` removes that variable and starts Electron cleanly. A small file that spares every future user a long headache. (`Launch NOVA.bat` does the same for people who prefer double-clicking.)
:::

::: whatif What if you start Electron with the variable set?
You see `TypeError: Cannot read properties of undefined (reading 'whenReady')` at the line `app.whenReady()`. `app` is undefined because `require('electron')` returned a path string instead of the API. Seeing this exact error tells you to look at the environment.
:::

::: check Checkpoint 3
A dark window appears, 1500 by 920 pixels, with a **40-pixel dark strip at the top** that has the standard window buttons at the right. Your text from `index.html` is below. Closing the window ends the program.
:::

## Step 4: The skeleton and the skin

**Goal.** The layout of an IDE appears: toolbar, side strip, project panel, editor area, bottom panel, status bar. No behaviour yet.

**Business rules.**

1. The editor must always get **all** the leftover space.
2. The bottom panel and the sidebar can be hidden and resized.
3. The look must support a dark and a light theme.
4. Everything must be reachable by keyboard.

### The HTML

NOVA's `renderer/index.html` has these major blocks (shortened):

```html
<div id="app">
  <header id="titlebar">…logo, project chip, run/debug/build/stop buttons…</header>

  <div id="workspace">
    <nav class="stripe" id="stripeLeft">…icon buttons…</nav>
    <aside id="sidebar">
      <div class="panel-head"><span id="sidebarTitle">Project</span>…</div>
      <div id="tree" class="scroll"></div>
      <div id="structure" class="scroll" hidden></div>
    </aside>
    <div class="splitter v" id="splitSide"></div>
    <main id="center">
      <div id="tabs"></div>
      <div id="crumbs"></div>
      <div id="editorWrap"><div id="editor"></div><div id="welcome">…</div></div>
    </main>
  </div>

  <div class="splitter h" id="splitBottom" hidden></div>
  <section id="bottom" hidden>…Run, Debug, Problems, Find, Terminal panes…</section>

  <footer id="status">…line:column, toolchain, language…</footer>
</div>
```

Compare it with the anatomy figure from Chapter 2: header (1), stripe (2), sidebar (3), tabs (4), editor (5), bottom (6), footer (7).

### The CSS grid

```css
#app { display: grid; grid-template-rows: 40px 1fr auto auto 24px; height: 100vh; }
#workspace { display: grid; grid-template-columns: 40px var(--side-w, 280px) 4px 1fr; min-height: 0; }
```

and the colour variables that give both themes:

```css
:root {
  --bg: #1e1f22;  --bg2: #2b2d30;  --text: #dfe1e5;  --accent: #3574f0;  /* …and more… */
}
:root[data-theme="light"] {
  --bg: #ffffff;  --bg2: #f7f8fa;  --text: #1e1f22;
}
[hidden] { display: none !important; }
```

::: why Why is the editor column 1fr and everything else fixed?
Because when the user resizes the window, the *editor* should absorb the change and the toolbars should keep their size. If you wrote percentages everywhere, the stripe would grow wide on a big monitor. `1fr` states the rule exactly.
:::

::: why Why a "splitter" element and not a CSS resize handle?
Each splitter is a 4-pixel strip. Dragging it runs a few lines of JavaScript that change `--side-w` (or `--bottom-h`). The grid recalculates by itself. It is a tiny example of *CSS and JavaScript cooperating through a variable*.
:::

### The icons

`renderer/icons.js` defines each icon as one SVG path string and a helper:

```js
window.ICONS = { play: 'M4.5 3l8 5-8 5z', stop: 'M4 4h8v8H4z', search: 'M7 12a5 5 0 100-10 5 5 0 000 10zM14 14l-3.4-3.4', /* … */ };
window.icon = (n) => `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.35"
  stroke-linecap="round" stroke-linejoin="round"><path d="${ICONS[n] || ''}"/></svg>`;
window.hydrateIcons = (root = document) =>
  root.querySelectorAll('i[data-i]').forEach(el => { if (!el.firstChild) el.innerHTML = icon(el.dataset.i); });
```

In the HTML you write `<i data-i="play"></i>`, and `hydrateIcons()` fills it with the SVG at start-up.

::: why Why generate icons from a table?
One short list holds all thirty icons, so the whole HTML stays tiny and readable, and changing the style of every icon (thicker lines, say) is a one-line edit.
:::

::: check Checkpoint 4
Your window shows the full IDE frame: the toolbar with buttons, the stripe with icons, a "No project open" area, a welcome card in the middle and a status bar at the bottom. The buttons do nothing yet. Clicking anywhere should not cause errors in the DevTools console (press **F12** if you added it, or run Electron with DevTools).
:::

{{img:shot-welcome.png|What the finished welcome screen looks like. Your Step 4 version will have the same layout but no recent projects and no working buttons yet.}}

## Troubleshooting Steps 1 to 4

| Symptom | Cause | Fix |
|---|---|---|
| White window, no layout | CSS path wrong in `index.html` | Check `<link href="styles.css">` and the file name |
| "Cannot read properties of undefined (reading 'whenReady')" | `ELECTRON_RUN_AS_NODE` is set | Start with `launch.js` / the `.bat` |
| Window opens but no buttons or icons | `icons.js` not loaded or `hydrateIcons` not called | Check the `<script>` tags and call `hydrateIcons()` after load |
| Bottom bar hangs off the screen | Missing `min-height: 0` on grid children | Add it to `#workspace` and the panels |
| Panel stays visible with `hidden` | Missing `[hidden]{display:none!important}` | Add that line |

## Mini assignments

::: assign Assignment 14.1: Verify your toolchain
Write a tiny Node script `doctor.js` that checks for `g++`, `gcc`, `cmake`, `ninja`, `gdb`, `clangd` and `git` using `child_process.spawnSync(where, [name])` and prints a tick or cross for each. (This is almost what NOVA's `tools:detect` does.)
:::

::: assign Assignment 14.2: Your own colours
Change `--accent` to a different colour and `--bg` to a deep blue. Relaunch. Make a third theme named `midnight` selected by `data-theme="midnight"`.
:::

::: assign Assignment 14.3: A custom title
Change the window title, the logo SVG colours and the product name. Which of these changes need a restart and which need only a reload (Ctrl+R in DevTools)?
:::

## Quiz

::: quiz
1. Why must the machine have g++ installed before NOVA works?
2. What is PATH?
3. What is the job of the `main` field in `package.json`?
4. Why does `launch.js` delete `ELECTRON_RUN_AS_NODE`?
5. Why use `backgroundColor` on the window?
6. What does `1fr` do in the layout grid?
7. Why are icons stored as SVG path strings?
8. What do `contextIsolation: true` and `nodeIntegration: false` protect?
:::

::: answers
1. NOVA does not contain a compiler; it starts the one on the machine.
2. A list of folders the system searches for programs.
3. It tells Electron which file to run first (the main process).
4. With that variable set Electron acts like plain Node and `require('electron')` returns nothing useful.
5. It is shown for the instant before the page paints, avoiding a white flash on a dark app.
6. It gives that row/column all the remaining free space.
7. They stay sharp at any size, take the text colour, and a single table keeps the HTML small.
8. They stop web content in the window from using Node's powers, such as reading and deleting files.
:::
