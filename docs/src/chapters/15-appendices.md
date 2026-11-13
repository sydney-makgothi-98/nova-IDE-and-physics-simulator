@@part Appendices

# Appendix A: Nomenclature (Words and Functions)

This is your dictionary. Each entry says **what it is**, **what it means in plain words**, and **why it matters**. Words are grouped by topic. Use the Contents page to jump here whenever a word is new.

## How to read this list

Every table has three columns: **Term** (the word or function), **What it is** (a plain-English meaning), and **Why it matters** (the reason you should care). When a word is a function, you will see brackets after its name.

## General IDE and programming words

| Term | What it is | Why it matters |
|---|---|---|
| IDE | Integrated Development Environment: one program with editor, build, debug and more | It is the workshop where you spend your working day |
| Source code | The human-readable text of a program | It is what you write; everything else is made from it |
| Compiler | Translates the whole program into machine code before it runs | Makes C/C++ programs fast; reports your mistakes |
| Interpreter | Runs a program line by line as it reads it | Starts instantly; used by JavaScript and Python |
| Linker | Joins compiled pieces and libraries into one program | Explains "undefined reference" errors |
| Library | Ready-made code you can use in your program | Saves you rewriting common things |
| Build | The whole process of turning source into a runnable program | The thing the Run and Build buttons start |
| Executable (.exe) | A finished program file | The result of a successful build |
| Debug build | A build with extra information for debuggers | Needed for stepping and breakpoints |
| Release build | A build optimised for speed, without debug information | What you ship to users |
| Syntax | The grammar rules of a language | A syntax error means you broke the grammar |
| Syntax highlighting | Colouring words by their role | Lets your eyes see structure |
| Token | A small meaningful piece of text (a number, a name, a symbol) | The first thing any language tool produces |
| Lexer | Cuts text into tokens | Step one of an interpreter or compiler |
| Parser | Arranges tokens into a tree by the grammar | Gives meaning to code |
| AST | Abstract Syntax Tree: the tree a parser builds | Used by compilers, language servers and refactoring |
| Refactoring | Changing code structure without changing behaviour | IDEs do it safely (rename, extract) |
| Snippet | A short template that expands into code | Saves typing common patterns |
| Breakpoint | A mark telling the debugger to pause on a line | The basic debugging tool |
| Stack frame | One function call's information on the call stack | The debugger's Frames list is a list of these |
| Call stack | The chain of functions that led to the current line | Shows how you got here |
| Watch | An expression the debugger re-evaluates at every stop | Track a value without hunting for it |
| Step over / into / out | Run one line / enter the function / leave it | The core debugger moves (F8 / F7 / Shift+F8) |
| Diagnostic | A message from a tool about a problem in your code | The red and yellow squiggles |
| Warning | A message about code that compiles but looks risky | Fixing warnings prevents bugs |
| Exit code | The number a program returns when it ends; 0 = success | Build tools and tests rely on it |
| Stdin / stdout / stderr | Input stream, normal output stream, error output stream | How programs talk to the console and to NOVA |
| Pipe | A one-way channel of bytes between two programs | How NOVA reads output from the compiler |
| Process | A running program with its own memory | NOVA runs many: main, window, g++, gdb... |
| Environment variable | A named setting inherited by programs (like PATH) | Explains many "works on my machine" problems |
| PATH | The list of folders searched for programs | If a tool is not on PATH, NOVA cannot find it |
| Terminal / shell | A text window / the program that reads your commands | Where command-line tools run |
| PTY / ConPTY | A pseudo-terminal that makes a program think a person is watching | Needed for a real integrated terminal |
| ANSI escape sequence | Hidden codes in text for colours and cursor moves | How terminals draw colours |
| Git | A tool that records the history of your files | Almost every project uses it |
| Race condition | A bug that depends on which of two things happens first | Hard to find; needs careful design |
| Undefined behaviour | Code the language does not define: anything may happen | The root of many C/C++ bugs |
| Memory leak | Memory that is taken and never given back | Makes programs slowly eat RAM |
| Dangling pointer | A pointer to memory that was freed | Crashes or security holes |

## Web words: HTML, CSS and the browser

| Term | What it is | Why it matters |
|---|---|---|
| HTML | Markup describing what is on a page | The skeleton of NOVA's window |
| Tag / element | A labelled piece such as `<p>...</p>` | The building block of HTML |
| Attribute | Extra information inside a tag (`id`, `class`, `href`) | Lets CSS and JavaScript find and configure elements |
| DOM | The tree of elements the browser builds | JavaScript changes the DOM to change the screen |
| CSS | Rules for how elements look | Colours, sizes, spacing, themes |
| Selector | The part of a CSS rule that chooses elements | Decides what a rule affects |
| Specificity | Which rule wins when two clash | Explains why "my style does nothing" |
| Box model | Content, padding, border, margin | Every layout problem starts here |
| Flexbox | CSS layout for rows or columns | Toolbars, tab bars |
| Grid | CSS layout for rows and columns together | The whole IDE frame |
| `fr` unit | A fraction of free space in a grid | Lets the editor take the leftover room |
| CSS variable | A named value such as `--bg` | Themes in one place |
| Pseudo-element | Generated content like `::before` | Spinner and decorations without extra HTML |
| SVG | Vector graphics described in text | Sharp icons and diagrams |
| Viewport | The visible area of the page | Layouts are measured against it |
| Event | Something that happens (click, key, resize) | How the UI reacts to the user |
| Event delegation | One listener on a parent handling many children | Efficient and works for new elements |
| Web worker | A background thread in the browser | Keeps heavy jobs from freezing the editor |
| Sandbox | A restricted area where web code runs | Why a web page cannot read your files |

## JavaScript words

| Term | What it is | Why it matters |
|---|---|---|
| `let` / `const` | Variable that can change / cannot change | Prefer const to avoid surprises |
| `===` | Strict equality (value and type) | Avoids hidden conversions |
| Arrow function | Short function syntax `(a) => a * 2` | Used everywhere in modern JS |
| Closure | A function that remembers its surrounding variables | Debounce, event handlers, private data |
| Callback | A function passed to be called later | The old style of asynchronous code |
| Promise | An object for a future value or error | The base of async code |
| `async` / `await` | Write promise code like normal code | Keeps the UI alive while waiting |
| Event loop | The mechanism that runs queued callbacks one at a time | Why JS never does two things at once |
| Template string | A backtick string with `${...}` | Readable text building |
| Destructuring | Pulling parts out of an object or array | Cleaner code |
| Spread `...` | Copy or expand arrays/objects | Safe copying and merging |
| Optional chaining `?.` | Stop safely if something is missing | Avoids "cannot read property of undefined" |
| Regex | A pattern for matching text | Parsing compiler output |
| JSON | A text format for data | Settings and protocols |
| Debounce | Wait until activity stops before acting | Fewer messages to the language server |
| Module | A bundle of code loaded with `require` or `import` | How large programs are split |
| Scope | The region where a name is visible | Source of many bugs when misunderstood |

## Node.js and Electron words

| Term | What it is | Why it matters |
|---|---|---|
| Node.js | JavaScript outside the browser | The engine of NOVA's main process |
| npm | Node's package installer | Downloads Electron, Monaco, etc. |
| `package.json` | Project description and dependency list | Tells npm and Electron what to do |
| Electron | Toolkit for desktop apps with web tech | Gives NOVA its window |
| Main process | The Node side of an Electron app | The only side that may touch the system |
| Renderer process | The web page side | Draws the UI, limited power |
| Preload script | Runs before the page; builds the safe bridge | Controls what the page can do |
| `contextBridge` | Electron's way to expose chosen functions to the page | The safe door |
| IPC | Messages between main and renderer | How the window asks for files |
| `ipcMain.handle` | Register an answer for a channel | The main process side of a question |
| `ipcRenderer.invoke` | Ask the main process and await the answer | The window side of a question |
| `webContents.send` | Main pushes a message to the window | Streaming output events |
| `BrowserWindow` | An application window | NOVA's main frame |
| Native module | A Node add-on written in C/C++ | `node-pty` is one; must match Electron's version |
| Monaco | The editor component of VS Code | NOVA's editor |
| xterm.js | A terminal emulator for the web | NOVA's terminal display |

## C and C++ words

| Term | What it is | Why it matters |
|---|---|---|
| Header file (`.h`) | A file listing what exists | Lets many files share declarations |
| `#include` | Paste another file's text here | How code is shared |
| Include guard | `#ifndef`/`#pragma once` protection | Stops double inclusion |
| Macro | Text substitution by the preprocessor | Powerful but dangerous |
| Pointer | A variable holding a memory address | The core of C |
| Reference | An alias for another variable (C++) | Pass without copying |
| Stack | Automatic memory for locals and calls | Fast, but small |
| Heap | Manual memory from `malloc`/`new` | Big, but you must free it |
| `NULL` / `nullptr` | A pointer pointing at nothing | Check before using a pointer |
| Struct | A group of variables under one name | Your own data type |
| Padding | Hidden bytes inserted for alignment | Explains sizeof surprises |
| Class | A type bundling data and functions (C++) | The core of C++ |
| Constructor / destructor | Runs when an object is created / destroyed | Set-up and tidy-up |
| RAII | Tie a resource's life to an object | The key C++ safety idea |
| Smart pointer | An object that owns and frees a pointer | Removes most manual `delete` |
| Inheritance | A class extending another | Reuse and polymorphism |
| Virtual function | Chosen at run time by the real type | Polymorphism |
| Template | Code generated for any type | `std::vector`, generic code |
| Namespace | A named box of names | Avoids name clashes |
| STL | The C++ standard library of containers and algorithms | Ready-made, fast, tested |
| Lambda | An inline unnamed function | Used with algorithms |
| Move semantics | Steal an object's insides instead of copying | Makes returning big objects cheap |
| `const` | A promise not to change something | Safer code, better optimisation |
| `constexpr` | Computed at compile time | Faster programs |
| UB sanitizer / ASan | Compiler tools that detect memory bugs at run time | Find bugs you would miss |

## Build and tool words

| Term | What it is | Why it matters |
|---|---|---|
| CMake | A build-system generator | The standard for C/C++ projects |
| `CMakeLists.txt` | CMake's recipe file | Describes targets and options |
| Target | A thing to build (program or library) | The unit CMake thinks in |
| Generator | The tool CMake writes files for (Ninja, Make) | Chosen with `-G` |
| Ninja | A very fast build tool | Quick incremental builds |
| Configure / generate | CMake reads the recipe and writes build files | The first of the two steps |
| Build folder | Where all generated files go | Keeps sources clean |
| `compile_commands.json` | The exact compiler command for each file | How clangd understands your code |
| LSP | Language Server Protocol | Standard messages between editor and language server |
| clangd | A C/C++ language server | NOVA's completion, hover, diagnostics |
| clang-format | A code formatter | One consistent style |
| gdb | The GNU debugger | NOVA's debugger |
| GDB/MI | gdb's machine-readable interface | How NOVA controls gdb |
| Token (MI) | A number sent before an MI command | Matches an answer to its command |
| Sanitizer | A tool that checks for bugs while the program runs | Finds leaks and overflows |
| Coverage | A report of which lines ran | Shows what tests miss |
| Profiler | A tool that measures where time is spent | Finds slow code |

## Most-used functions and commands

### JavaScript and the browser

| Function | What it does | Why it matters |
|---|---|---|
| `console.log(x)` | Prints x | Your first debugging tool |
| `document.querySelector(css)` | Finds the first matching element | How code reaches the page |
| `element.addEventListener(type, fn)` | Runs fn on an event | Makes the UI interactive |
| `element.classList.toggle(name)` | Adds or removes a class | Switch looks and states |
| `element.dataset.name` | Reads a `data-name` attribute | Per-element information |
| `array.map(fn)` | New array with fn applied to each item | Transform lists |
| `array.filter(fn)` | Items for which fn is true | Pick from lists |
| `array.find(fn)` | First item matching | Look up an item |
| `array.reduce(fn, start)` | Combine to one value | Sums, counts, groups |
| `setTimeout(fn, ms)` | Run fn later | Delays, debounce |
| `JSON.parse(text)` / `JSON.stringify(obj)` | Text to data / data to text | Settings and messages |
| `Promise.all([...])` | Wait for several promises | Parallel jobs |
| `str.match(regex)` / `str.replace(re, x)` | Find / change text by pattern | Text parsing |
| `Object.keys(obj)` | List of property names | Loop over objects |

### Node.js and Electron

| Function | What it does | Why it matters |
|---|---|---|
| `require('fs')` | Loads the file module | Access to the disk |
| `fs.promises.readFile(path, 'utf8')` | Reads a text file without blocking | Open files |
| `fs.promises.writeFile(path, text)` | Writes a file | Save files |
| `fs.promises.readdir(dir, {withFileTypes:true})` | Lists a folder | The file tree |
| `path.join(a, b)` | Joins parts with the right separator | Portable paths |
| `child_process.spawn(cmd, args)` | Starts a program with streams | Compilers, debuggers |
| `spawnSync('where', [name])` | Starts and waits | Detect tools |
| `process.env` / `process.platform` | Environment / OS name | Platform-specific code |
| `app.whenReady()` | Resolves when Electron is ready | Start of every app |
| `new BrowserWindow(options)` | Creates a window | The UI frame |
| `ipcMain.handle(channel, fn)` | Answers window questions | Main-process services |
| `ipcRenderer.invoke(channel, ...args)` | Asks the main process | Window-side calls |
| `contextBridge.exposeInMainWorld(name, api)` | Offers functions to the page | The safe bridge |
| `dialog.showOpenDialog(win, opts)` | Native file/folder picker | Open Folder |

### C

| Function | What it does | Why it matters |
|---|---|---|
| `printf(fmt, ...)` | Prints formatted text | Output and debugging |
| `scanf` / `fgets(buf, n, stdin)` | Reads input (prefer `fgets`) | Safe input |
| `malloc(n)` / `free(p)` | Borrow / return heap memory | Dynamic data |
| `realloc(p, n)` | Resize heap memory | Growing arrays |
| `strlen(s)` | Length of a string | Needed everywhere |
| `strcpy`, `strcat`, `strcmp` | Copy, join, compare strings | Beware buffer sizes |
| `snprintf(buf, n, fmt, ...)` | Safe formatted write | Prevents overflow |
| `fopen`, `fgets`, `fprintf`, `fclose` | File input/output | Read and write files |
| `qsort(base, n, size, cmp)` | Sort anything | Callbacks in action |
| `memcpy`, `memset` | Copy / fill bytes | Fast block work |
| `exit(code)` | End the program | Exit codes |

### C++

| Function / type | What it does | Why it matters |
|---|---|---|
| `std::cout << x` | Prints x | Output |
| `std::string` | A safe string | No buffer overflows |
| `std::vector<T>` | A growable array | The default container |
| `v.push_back(x)` / `v.size()` | Add at the end / count | Everyday use |
| `std::map<K,V>` / `std::unordered_map<K,V>` | Key to value | Lookups |
| `std::sort(b, e)` | Sorts a range | Algorithms beat hand loops |
| `std::find`, `std::count_if`, `std::accumulate` | Search, count, sum | Common tasks |
| `std::make_unique<T>(args)` | Creates a uniquely owned object | Safe ownership |
| `std::make_shared<T>(args)` | Creates a shared object | Shared ownership |
| `std::move(x)` | Allow x to be moved from | Avoids copies |
| `std::optional<T>` | A value that may be missing | Safer than magic values |
| `std::thread`, `std::mutex`, `std::lock_guard` | Threads and locks | Concurrency |
| `std::ifstream` / `std::ofstream` | File streams (RAII) | Files that close themselves |

### CMake commands

| Command | What it does | Why it matters |
|---|---|---|
| `cmake_minimum_required(VERSION x)` | Pins rules | Stable behaviour |
| `project(name LANGUAGES CXX)` | Names the project | Always required |
| `add_executable(t files...)` | Defines a program | The usual target |
| `add_library(t STATIC files...)` | Defines a library | Reusable code |
| `target_link_libraries(t PRIVATE lib)` | t uses lib | Links targets |
| `target_include_directories(t PUBLIC dir)` | Header search folders | Fixes "no such file" |
| `target_compile_options(t PRIVATE -Wall)` | Compiler flags for t | Per-target options |
| `set(var value)` / `option(NAME "doc" ON)` | Variable / switch | Configuration |
| `add_subdirectory(dir)` | Include another folder's recipe | Large projects |
| `enable_testing()` / `add_test()` | Testing with CTest | Automated checks |

### gdb commands (typed in NOVA's debug console)

| Command | What it does | Why it matters |
|---|---|---|
| `break file.cpp:12` | Set a breakpoint | Stop where you choose |
| `run` / `continue` | Start / resume | Move forward |
| `next` / `step` / `finish` | Over / into / out | Line by line |
| `print expr` | Show a value | Inspect state |
| `bt` | Backtrace (call stack) | See how you got here |
| `info locals` | All local variables | Quick overview |
| `watch var` | Stop when var changes | Find who changes it |
| `x/8xw &var` | Examine memory | See raw bytes |

### Most-used CSS properties

| Property | What it does | Why it matters |
|---|---|---|
| `display: flex / grid / none` | Layout mode or hide | Foundation of layout |
| `gap`, `padding`, `margin` | Spacing | Breathing room |
| `background`, `color`, `border` | Paint | Visual identity |
| `width`, `height`, `min-*`, `max-*` | Size limits | Resizable panels |
| `overflow: auto` | Scrollbars when needed | Long lists |
| `position: absolute / fixed` | Place freely | Popups, overlays |
| `transition`, `animation` | Motion | Polish |
| `var(--name)` | Use a variable | Themes |

# Appendix B: Cheat Sheets and Troubleshooting

## NOVA keyboard shortcuts

| Keys | Action |
|---|---|
| **Shift + F10** | Run |
| **Shift + F9** | Debug |
| **Ctrl + F9** | Build |
| **Ctrl + F2** | Stop |
| **F8 / F7 / Shift + F8** | Step over / into / out (while debugging) |
| **F9** | Resume (while debugging) |
| **Alt + F9** | Run to cursor |
| **Ctrl + F8** | Toggle breakpoint |
| **Shift, Shift** | Search Everywhere |
| **Ctrl + Shift + A** | Actions palette |
| **Ctrl + P** or **Ctrl + Shift + N** | Go to file |
| **Ctrl + F12** | Go to symbol in this file |
| **Ctrl + Shift + F** | Find in files |
| **F2** | Rename (symbol in editor, or file in the tree) |
| **Ctrl + click** | Go to definition |
| **Shift + F12** | Find references |
| **Ctrl + .** | Quick fix |
| **Ctrl + Alt + L** | Reformat code |
| **Ctrl + S / Ctrl + Shift + S** | Save / save all |
| **Ctrl + W** | Close tab |
| **Ctrl + Tab** | Next tab |
| **Alt + 1 / 7** | Project / Structure panel |
| **Alt + 4 / 5 / 6 / F12** | Run / Debug / Problems / Terminal |
| **Ctrl + Alt + S** | Settings |
| **F12** | Developer tools (for debugging NOVA itself) |

## Command-line cheat sheet

```text
gcc  main.c   -o app.exe          build a C file
g++  main.cpp -o app.exe          build a C++ file
g++ -g -O0 -Wall -Wextra -std=c++17 main.cpp -o app.exe     the flags NOVA uses
cmake -S . -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug
gdb app.exe                        start the debugger by hand
clang-format -i main.cpp           format a file
node file.js                       run JavaScript
npm install                        install the libraries in package.json
npm start                          start NOVA
```

## Troubleshooting table

| Symptom | Likely cause | What to do |
|---|---|---|
| NOVA window does not appear | `ELECTRON_RUN_AS_NODE` set | Start with `Launch NOVA.bat` or `npm start` (uses `launch.js`) |
| "g++ not found" / no compiler in the status bar | Not on PATH | Add `C:\msys64\mingw64\bin` to PATH, reopen NOVA |
| Run does nothing, no message | Active file is not `.c` / `.cpp` and no CMake target | Open a C/C++ file or a project with `CMakeLists.txt` |
| Output of `printf` appears only at the end | Buffered output | Keep *Unbuffered console output* ticked in Settings |
| Squiggles under every `#include <...>` | clangd cannot find the standard headers | Ensure g++ is on PATH; check `--query-driver`; restart clangd |
| Squiggles appear on correct code | Wrong compile flags | Reconfigure CMake; delete `cmake-build-debug`; Actions → Reload CMake Project |
| "undefined reference" | A `.cpp` is not in `add_executable` | Add the file and reconfigure |
| Debug button says "gdb not found" | gdb not installed | `pacman -S mingw-w64-x86_64-gdb`, restart NOVA |
| Debugger skips lines or shows `??` | Optimised build or missing `-g` | Use Debug builds (NOVA does) |
| Program cannot read input while debugging | Inline console mode | Tick *Debug: run in its own console window* |
| Terminal shows "unavailable" | `node-pty` did not load | Run `npm install` again; ensure the Electron version matches |
| Blank editor, console errors about workers | Monaco worker URL problem | Check the `getWorkerUrl` code and the `node_modules` path |
| `Terminal is not defined` | xterm script loaded after Monaco's loader | Load xterm scripts **before** `loader.js` |
| Panel will not hide | Missing `[hidden]` rule | Add `[hidden] { display: none !important; }` |
| `npm start` opens Electron's default window | `main` missing in `package.json` | Set `"main": "main.js"` |
| `Cannot find module 'node-pty'` | Not installed | `npm install node-pty` |

::: tip How to debug NOVA itself
Press **F12** to open Chromium's developer tools for the window. The **Console** tab shows errors from the renderer files. Type `S` to see NOVA's whole state. For the main process, look at the terminal where you ran `npm start`: `console.log` from `main.js` appears there.
:::

# Appendix C: Roadmaps for the Big Assignments

These are *roadmaps*, not solutions. Each lists milestones in a sensible order. Finish one milestone, test it, then move on. Never write 300 lines before running anything.

## MiniIDE: HTML (Chapter 6)

1. Write the outer shell: header, nav, aside, main, section, footer, with no content.
2. Fill the header (logo SVG, three buttons).
3. Build the nested `ul` tree. Check that it indents.
4. Add the tabs and the code block.
5. Add the four bottom panels, with only one visible.
6. Add `id`/`data-` attributes and `alt`/`aria-label` text.
7. Validate: open the file; read it top to bottom; compare with `renderer/index.html`.

## MiniIDE: CSS (Chapter 7)

1. Add the variables and the `[hidden]` and `box-sizing` rules.
2. Make `body` fill the window and create the outer grid.
3. Make the middle area its own grid.
4. Style the tree, then the tabs, then the buttons.
5. Add the second theme and a temporary toggle.
6. Add the spinner animation.
7. Shrink the window to 700px and fix overlaps.

## MiniIDE: JavaScript (Chapter 8)

1. Create the `S` state object and the fake project.
2. Draw the tree from the object.
3. Open tabs; show the file in the textarea.
4. Add dirty tracking and Ctrl+S.
5. Add the Run button with the `wait()`-based fake build, then Stop.
6. Add find in files (with debounce) and the Problems panel.
7. Add theme and settings with `localStorage`.
8. Add shortcuts. Finally, check the console for errors.

## Folder Peek: Node and Electron (Chapter 9)

1. Hello Electron window with the three files.
2. Add the preload and expose one function; prove `require` is blocked.
3. Add *Open Folder* and `readDir` (folders first).
4. Draw the tree; load children lazily.
5. Read and show files; add save.
6. Add Run with `spawn`: stdout, stderr, exit code, Stop.
7. Handle errors (missing program, binary file). Write the README.

## Library manager: C (Chapter 10)

1. Define `Book` and the growable array; test with hard-coded books.
2. Write `add` and `list`.
3. Write the menu loop with safe input reading.
4. Add search (case-insensitive), check out and return.
5. Add `qsort` with three comparers.
6. Add save and load; test with a missing and a damaged file.
7. Split into `book.h/.c` and `main.c`; add include guards; build with CMake.
8. Compile with `-Wall -Wextra`; run under the debugger; check for leaks by reasoning about every `malloc`.

## Outline: C++ (Chapter 11)

1. Take the tokenizer from the chapter and add line numbers.
2. Add multi-line comments and preprocessor lines.
3. Print all tokens with their lines to prove the lexer.
4. Track brace depth.
5. Find classes, then functions; store `Symbol` objects.
6. Print the aligned outline and the summary map.
7. Split into files, add error handling, build with CMake, run on your own sources.

## GeoKit: CMake (Chapter 12)

1. Create the folders and an empty top-level recipe.
2. Add the library with two classes; build it alone.
3. Add the executable and link; get it to print one area.
4. Add file reading.
5. Add the tests executable and `add_test`; make one test fail on purpose to see ctest report it.
6. Add the options and test each with `-D...`.
7. Open in NOVA; check the run configurations; debug into the library.

## EchoLS: Protocols (Chapter 13)

1. Write the framing reader and writer; test with a hand-made frame.
2. Handle `initialize` and `shutdown`; test from a small client.
3. Track documents (`didOpen`/`didChange`).
4. Implement hover.
5. Implement completion.
6. Publish diagnostics.
7. Write the client with the `pending` map and timeouts; test every feature.
8. Add error replies for unknown methods.

::: tip The golden rule of every roadmap
Always keep something working. After every milestone, run the program. If you write for an hour without running it and it fails, you will not know where the problem is. Small steps, run often, fix at once.
:::
