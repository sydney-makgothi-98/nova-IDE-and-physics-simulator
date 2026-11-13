# Node.js and Electron: The Engine Room

## Why we need them

A normal web page is locked in a safe room called the **sandbox**. It cannot read your files, start programs or use your hard disk. That keeps you safe when you visit a website. But an IDE *must* read files, start compilers and talk to debuggers.

Two tools open the safe room, carefully:

* **Node.js** lets JavaScript run on your computer, outside the browser, with powers to read files, start programs and use the network.
* **Electron** bundles Node.js together with the Chromium browser engine so you can build a normal desktop program (with its own window and icon) out of web technology.

::: def Module
A **module** is a ready-made bundle of functions that you load with `require`. Node ships with many built-in modules: `fs` (files), `path` (file names), `child_process` (start programs), `os` (information about the computer), `events` (messaging).
:::

## Node basics

Install Node from nodejs.org, then in a terminal type `node --version`. Save any JavaScript file and run it with `node file.js`. Everything from Chapter 8 works, and now you also have these modules.

### path: working with file names

Never build file names by gluing strings with `+ "/" +`. Windows uses backslashes, other systems use forward slashes. The `path` module hides the difference.

```js run
const path = require("path");

console.log(path.posix.join("src", "main.cpp"));
console.log(path.win32.join("src", "main.cpp"));
console.log(path.posix.basename("/home/me/project/main.cpp"));
console.log(path.posix.dirname("/home/me/project/main.cpp"));
console.log(path.posix.extname("main.cpp"));
console.log(path.posix.basename("main.cpp", ".cpp"));
console.log(path.posix.resolve("/a/b", "../c"));
```

::: why Why not just use "a" + "/" + "b"?
Because the right slash depends on the operating system, and doubled or missing slashes sneak in. `path.join` fixes all of that, even removes accidental doubles, and `path.dirname`/`basename` cut paths apart correctly. NOVA's own `join`, `base` and `dirname` helpers in `app.js` exist because the *window* (renderer) cannot use `path`, so they were written by hand to do the same job.
:::

### fs: reading and writing files

```js run
const fs = require("fs");
const os = require("os");
const path = require("path");

const dir = fs.mkdtempSync(path.join(os.tmpdir(), "nova-demo-"));   // a fresh empty folder
const file = path.join(dir, "hello.txt");

fs.writeFileSync(file, "Hello from Node!\nSecond line.\n");
const text = fs.readFileSync(file, "utf8");
console.log(text.split("\n").length - 1, "lines:", JSON.stringify(text));

console.log(fs.existsSync(file), fs.readdirSync(dir));
fs.mkdirSync(path.join(dir, "sub"));
console.log(fs.readdirSync(dir, { withFileTypes: true }).map(e => (e.isDirectory() ? "[dir] " : "") + e.name));
console.log(fs.statSync(file).size, "bytes");

fs.rmSync(dir, { recursive: true, force: true });                     // tidy up
console.log("exists after delete:", fs.existsSync(dir));
```

Notice `Sync` at the end of every function name. Those functions **block** (the waiter stands at the kitchen). They are fine for short scripts. In a program with a window you use the promise versions so that the program is never frozen:

```js run
const fs = require("fs");
const os = require("os");
const path = require("path");

async function main() {
  const dir = await fs.promises.mkdtemp(path.join(os.tmpdir(), "nova-demo-"));
  const file = path.join(dir, "note.txt");
  await fs.promises.writeFile(file, "async is polite");
  console.log(await fs.promises.readFile(file, "utf8"));
  await fs.promises.rm(dir, { recursive: true, force: true });
}
main();
```

This is exactly how NOVA's main process reads files for the window:

```js
ipcMain.handle('fs:read', async (_e, f) => {
  const buf = await fs.promises.readFile(f);
  if (buf.includes(0)) return { binary: true };
  return { text: buf.toString('utf8') };
});
```

::: why Why does NOVA check buf.includes(0)?
Text files never contain the byte `0`, but program files (`.exe`), images and other binary files are full of them. If NOVA tried to show a `.exe` as text, the editor would display garbage and might hang. A single cheap check protects the whole editor.
:::

### os and process

```js run
const os = require("os");
console.log(typeof os.platform(), typeof os.homedir(), os.EOL === "\n" || os.EOL === "\r\n");
console.log(process.argv.length >= 2, typeof process.env.PATH, typeof process.pid);
console.log(process.cwd().length > 0);
```

`process.argv` holds the command-line arguments, `process.env` the environment variables (including `PATH`, the list of folders where programs are found) and `process.cwd()` the folder you started in.

## Events: EventEmitter

Much of Node is built on **events**: "tell me when X happens".

```js run
const { EventEmitter } = require("events");

const bus = new EventEmitter();
bus.on("saved", (name) => console.log("listener A: saved", name));
bus.on("saved", (name) => console.log("listener B: refresh tree for", name));
bus.once("opened", (name) => console.log("opened (only the first time):", name));

bus.emit("saved", "main.cpp");
bus.emit("opened", "main.cpp");
bus.emit("opened", "util.cpp");   // nobody listens any more
```

::: why Why events?
They let one part of a program announce something **without knowing who cares**. When a file is saved, the code that saves it does not need to know that the tab bar, the tree, clangd and the build system all want to know. They each subscribe. Adding a new listener never changes the saving code. This is how big programs stay flexible.
:::

## Starting other programs: child_process

This is the **most important Node module for an IDE**. It lets you start a program (the compiler, git, the debugger), feed it input and read its output.

```js run
const { spawn } = require("child_process");

// Start a child program: here we use Node itself as the "program".
const child = spawn(process.execPath, ["-e", "console.log('hello from child'); console.error('a warning'); process.exit(3)"]);

let out = "", err = "";
child.stdout.on("data", (chunk) => (out += chunk));
child.stderr.on("data", (chunk) => (err += chunk));
child.on("close", (code) => {
  console.log("stdout:", out.trim());
  console.log("stderr:", err.trim());
  console.log("exit code:", code);
});
```

Every program has three standard streams:

| Stream | Name | Used for |
|---|---|---|
| `stdin` | standard input | Text going **into** the program (keyboard or a pipe) |
| `stdout` | standard output | Normal text coming **out** |
| `stderr` | standard error | Error and warning text coming out |

The **exit code** is a number the program returns when it ends: **0 means success**, anything else means failure. NOVA prints it at the end of every run: *"Process finished with exit code 0"*.

{{fig:stream-flow|A child program writes to two pipes. Our program listens to 'data' events as they arrive.}}

::: why Why are stdout and stderr separate?
So you can treat them differently. NOVA draws stderr text in red. You can also save the normal output to a file and still see errors on the screen. Compilers print their error messages on stderr for exactly this reason.
:::

### Output arrives in chunks

```js run
const { spawn } = require("child_process");
const script = "let n=0; const t=setInterval(()=>{ process.stdout.write('part '+(++n)+'\\n'); if(n==3){clearInterval(t)} }, 50)";
const child = spawn(process.execPath, ["-e", script]);
let chunks = 0;
child.stdout.on("data", (c) => { chunks++; console.log("received:", c.toString().trim()); });
child.on("close", () => console.log("chunks:", chunks));
```

Data does not arrive as neat whole lines. A line may be split across two chunks, or two lines can arrive in one chunk. This is why NOVA keeps a **buffer** and only parses *complete* lines (see `execStep` in `app.js`).

::: whatif What if you parse each chunk as if it were a line?
Sometimes a compiler error such as `main.cpp:14:9: error: 'x' was not declared` arrives split into `main.cpp:14:9: err` and `or: 'x' was not declared`. The pattern would match neither piece, and the error would silently vanish. Buffering until the newline is the cure.
:::

### Worked example: a tiny process runner

NOVA wraps `spawn` in a helper so the rest of the code can simply `await` the result. Here is the same idea:

```js run
const { spawn } = require("child_process");

function runCommand(cmd, args, input) {
  return new Promise((resolve) => {
    const child = spawn(cmd, args);
    let stdout = "", stderr = "";
    child.stdout.on("data", d => (stdout += d));
    child.stderr.on("data", d => (stderr += d));
    child.on("error", (e) => resolve({ code: -1, stdout, stderr: stderr + e.message }));
    child.on("close", (code) => resolve({ code, stdout, stderr }));
    if (input !== undefined) { child.stdin.write(input); child.stdin.end(); }
  });
}

async function main() {
  const r1 = await runCommand(process.execPath, ["-e", "console.log(1 + 1)"]);
  console.log(r1.code, JSON.stringify(r1.stdout));

  const r2 = await runCommand(process.execPath, ["-e", "process.stdin.on('data', d => console.log(d.toString().toUpperCase()))"], "shout this");
  console.log(r2.code, JSON.stringify(r2.stdout.trim()));

  const r3 = await runCommand("no-such-program-xyz", []);
  console.log(r3.code, r3.stderr.includes("ENOENT"));
}
main();
```

Study `r3`. Starting a program that does not exist does not crash Node; it fires an `'error'` event. NOVA handles it by printing *"Failed to start"* in the Run panel. If the `'error'` handler were missing, Node would throw an uncaught exception and the whole IDE would crash.

::: warn Watch out
Always handle the `'error'` event on a child process. It is the most common cause of mysterious crashes in tools that start other programs.
:::

## Electron: a window made of web technology

### The two process types again

We met them in Chapter 4: a **main process** (Node, full powers) and **renderer processes** (web pages, limited powers). Here is the smallest Electron program that opens a window. It needs three files in one folder.

`package.json`:

```json
{
  "name": "hello-electron",
  "version": "1.0.0",
  "main": "main.js",
  "scripts": { "start": "electron ." },
  "dependencies": { "electron": "^45.0.0" }
}
```

`main.js`:

```js
const { app, BrowserWindow } = require('electron');

function createWindow() {
  const win = new BrowserWindow({ width: 900, height: 600 });
  win.loadFile('index.html');
}
app.whenReady().then(createWindow);
app.on('window-all-closed', () => app.quit());
```

`index.html`:

```html
<!doctype html>
<html><body><h1>Hello from Electron!</h1></body></html>
```

Run `npm install` once, then `npm start`. A real window appears.

Line by line:

* `app` represents the whole program: its life from start to quit.
* `app.whenReady()` waits until Electron has finished starting up. Creating a window earlier would fail.
* `new BrowserWindow({...})` makes a window. `loadFile` fills it with your HTML page.
* `window-all-closed` fires when the user closes the last window. We quit, so the program does not linger invisibly.

### Making a window look modern

NOVA's real window options, from `main.js`:

```js
win = new BrowserWindow({
  width: 1500, height: 920, minWidth: 900, minHeight: 600,
  backgroundColor: '#1e1f22',
  frame: false,
  titleBarStyle: 'hidden',
  titleBarOverlay: { color: '#2b2d30', symbolColor: '#dfe1e5', height: 40 },
  webPreferences: { preload: path.join(__dirname, 'preload.js'),
                    contextIsolation: true, nodeIntegration: false }
});
```

::: why Why each option?
* `backgroundColor` is shown for the instant before the page paints. Without it you see a white flash on a dark app, which feels cheap.
* `frame: false` and `titleBarStyle: 'hidden'` remove the standard grey title bar, so NOVA can draw its own toolbar there.
* `titleBarOverlay` keeps the native minimise, maximise and close buttons, painted in NOVA's colours. We get the modern look without rebuilding those buttons.
* `minWidth` and `minHeight` stop the user from shrinking the window until the layout breaks.
:::

## The safe bridge: preload and contextBridge

Now the most important security idea in Electron.

```js
// main.js
webPreferences: { preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false }
```

* `nodeIntegration: false` means the web page **cannot** use `require`, `fs` or `child_process`.
* `contextIsolation: true` means the preload script runs in a **separate world** from the page, so the page cannot tamper with it.

The preload script is allowed to use Electron's APIs, and it hands the page only a small, named set of functions:

```js
// preload.js
const { contextBridge, ipcRenderer } = require('electron');
const inv = (ch) => (...a) => ipcRenderer.invoke(ch, ...a);
contextBridge.exposeInMainWorld('nova', {
  read: inv('fs:read'),
  write: inv('fs:write'),
  readDir: inv('fs:readDir'),
  start: inv('proc:start'),
  kill: inv('proc:kill'),
  onData: (cb) => ipcRenderer.on('proc:data', (_e, m) => cb(m)),
});
```

After this, any script in the window can call `window.nova.read(path)` and **nothing else** from Node. The helper `inv` is a closure factory: it turns the channel name into a function that forwards its arguments.

::: whatif What if nodeIntegration were true?
A script in your page could write `require('child_process').exec('del /s /q C:\\*')`. If your IDE ever displayed one malicious string as HTML (for example a hostile file name or compiler message), your computer could be wiped. With the bridge, the worst a page can do is call the few functions you chose to expose, and each can be written to check its arguments.
:::

### invoke and handle: asking and answering

The main process registers **handlers** for channel names:

```js
// main.js
ipcMain.handle('fs:readDir', async (_e, dir) => {
  const ents = await fs.promises.readdir(dir, { withFileTypes: true });
  return ents
    .filter(d => !IGNORE.has(d.name))
    .map(d => ({ name: d.name, path: path.join(dir, d.name), dir: d.isDirectory() }))
    .sort((a, b) => (b.dir - a.dir) || a.name.localeCompare(b.name));
});
```

The window then writes `const files = await nova.readDir(folder)`. The reply is whatever the handler returns, delivered through a promise.

::: why Why sort inside the main process?
The sort puts folders before files and then orders alphabetically, ignoring capital letters. `(b.dir - a.dir)` uses a trick: `true - false` is `1`, so directories rise to the top. Doing it once, in the engine room, means every part of the window gets a tidy list for free.
:::

### Pushing messages: send

Some information flows the *other* way, not as an answer but as news: a program has printed a line. The main process pushes it:

```js
// main.js
child.stdout.on('data', d =>
  win.webContents.send('proc:data', { id, stream: 'out', data: d.toString() }));
```

and the bridge forwards it to a listener the window registered with `nova.onData(callback)`.

### Worked example: a mini IPC bus you can run

You cannot run Electron's IPC outside Electron, but you can build the *idea* with a few lines and see why `invoke` and `handle` feel like function calls:

```js run
const handlers = new Map();
const ipcMain = { handle: (ch, fn) => handlers.set(ch, fn) };
const ipcRenderer = { invoke: async (ch, ...args) => {
  const fn = handlers.get(ch);
  if (!fn) throw new Error("No handler for " + ch);
  return fn({}, ...args);
} };

// "main process" side
ipcMain.handle("math:add", async (_e, a, b) => a + b);
ipcMain.handle("fs:fakeRead", async (_e, name) => ({ text: "contents of " + name }));

// "window" side
(async () => {
  console.log(await ipcRenderer.invoke("math:add", 20, 22));
  console.log(await ipcRenderer.invoke("fs:fakeRead", "main.cpp"));
  try { await ipcRenderer.invoke("nope"); } catch (e) { console.log("error:", e.message); }
})();
```

The real thing also copies your arguments between processes (so the two sides never share memory), but the shape is identical.

## Dialogs, menus and other native features

```js
const { dialog } = require('electron');
const r = await dialog.showOpenDialog(win, { properties: ['openDirectory'] });
if (!r.canceled) console.log(r.filePaths[0]);
```

This opens the operating system's own folder picker. NOVA uses it for *Open Folder*. Because it is native, it looks right on every system and is familiar to the user.

## A story about two bugs

::: story The ELECTRON_RUN_AS_NODE trap
While building NOVA, the first launch printed `Cannot read properties of undefined (reading 'whenReady')`. The cause: the shell had an environment variable `ELECTRON_RUN_AS_NODE=1` set, which tells Electron to behave like plain Node, so `require('electron')` returned nothing useful. NOVA's `launch.js` and `Launch NOVA.bat` now clear that variable before starting. Lesson: *when a tool behaves strangely, check its environment.*
:::

::: story The silent compiler
Another time, g++ failed with no message at all when started from one particular terminal, yet worked in another. The two terminals had different environments. The lesson: an IDE runs programs in the environment *it* was started with. NOVA passes `env` explicitly to every child (`env: { ...process.env, ...opts.env }`) so it can be controlled and inspected.
:::

## Packaging (a first look)

To give NOVA to colleagues without asking them to install Node, you "package" it into an installer. The common tool is **electron-builder** or **electron-forge**. A basic command looks like `npx electron-builder --win`. It produces a `.exe` installer. Chapter 17 lists this as a first "next step".

## Mini assignments

::: assign Assignment 9.1: Folder lister
Write a Node script that takes a folder name as `process.argv[2]` and prints every file inside it (recursively), indented by depth, folders first.
:::

::: assign Assignment 9.2: Run and time
Write `timeIt(cmd, args)` that uses `spawn`, measures how long the program ran, and prints exit code, duration and the number of output lines.
:::

::: assign Assignment 9.3: Line buffer
Write a `LineBuffer` class with a method `push(chunk)` that returns only the *complete* lines found so far, and keeps the unfinished tail for next time. Test it with chunks that split lines in the middle.
:::

::: assign Assignment 9.4: Hello, Electron
Build the three-file Electron app from this chapter. Then add a button in `index.html` and a preload bridge so that clicking it shows the current time returned from the main process.
:::

## Big assignment: A file explorer with a Run button

::: big "Folder Peek": your first desktop app
**Goal.** Build a small Electron app that lets you browse a project and run Node scripts, using only what you have learned.

**Requirements.**

1. A frameless-style window with a custom title bar (`titleBarOverlay`), a dark `backgroundColor` and sensible `minWidth`/`minHeight`.
2. `contextIsolation: true` and `nodeIntegration: false`. The preload exposes exactly six functions through `contextBridge` (open folder, list directory, read file, write file, run a command, kill a command) and nothing else.
3. *Open Folder* uses `dialog.showOpenDialog`. The list-directory handler returns folders first, sorted by name, ignoring `node_modules` and `.git`.
4. The window shows the folder as an expandable tree (build it with the DOM from Chapter 8) and shows the selected text file in a `<textarea>`. Binary files show a friendly message.
5. *Save* (Ctrl+S) writes the file. A dirty dot appears while edited.
6. A *Run* button for `.js` files starts `node thatfile.js` with `spawn`, streams stdout (normal) and stderr (red) into a panel, and shows the exit code at the end. A *Stop* button kills it.
7. If the program does not exist or fails to start, show an error message in the panel instead of crashing.
8. Keep **all** paths in the main process built with `path.join`.
9. Write a short README explaining each IPC channel you created.

**Stretch goals.** Remember the last folder (store it as a JSON file in `app.getPath('userData')`). Add a stdin box to send input to the running script. Add find-in-files using a recursive `fs` walk in the main process. Package it with electron-builder.

**How to check yourself.** Open the DevTools console in your window (`win.webContents.openDevTools()`) and try typing `require('fs')`. It must say `require is not defined`. That proves your window is safe. Compare your handlers with `main.js` in NOVA.
:::

## Quiz

::: quiz
1. Why can't a normal web page read your files?
2. What do `Sync` functions do that the promise versions do not?
3. What are stdin, stdout and stderr?
4. What does an exit code of 0 mean?
5. Why must you handle the `'error'` event on a child process?
6. What do `nodeIntegration: false` and `contextIsolation: true` achieve?
7. What is `contextBridge.exposeInMainWorld` for?
8. What is the difference between `ipcRenderer.invoke` and `webContents.send`?
9. Why does NOVA buffer output before parsing it for errors?
10. Why use `path.join` instead of joining strings?
:::

::: answers
1. The browser sandbox forbids it for your safety.
2. `Sync` functions block (freeze) the program until finished; promise versions let it do other things while waiting.
3. Standard input (text going in), standard output (normal text coming out), standard error (error text coming out).
4. The program succeeded.
5. If the program cannot start (for example it does not exist), Node fires `'error'`; with no handler Node throws and the whole IDE can crash.
6. The page cannot use Node's powers directly, and the preload script is protected from the page's own scripts.
7. To give the page a small, named set of functions from the preload script, nothing more.
8. `invoke` is the window asking the main process a question and getting an answer; `send` is the main process pushing news to the window.
9. Output arrives in arbitrary chunks, so lines can be split; only complete lines can be matched reliably.
10. It uses the correct separator for the operating system and removes accidental double separators.
:::
