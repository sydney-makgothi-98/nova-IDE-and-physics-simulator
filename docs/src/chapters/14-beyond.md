@@part Part IV — Going Further

# Beyond NOVA: Bigger Projects to Build Next

## You are further along than you think

If you reached this page after building NOVA, you have used six languages, three protocols and about a dozen big ideas: processes, pipes, events, promises, state, parsing, memory, builds and debugging. Most working programmers never touch half of that.

This chapter gives you **projects to grow into**. For each one you get: what it is, why it is worth doing, what you will learn, the basic first steps, and what to read next. They are ordered roughly from easiest to hardest.

{{fig:progress-ladder|The same ladder you climbed in Part III. The projects below are the next rungs.}}

## The menu of projects

| # | Project | Difficulty | You will learn | Builds on |
|---|---|---|---|---|
| 1 | Git panel (status, diff, commit) | Easy | Running tools, parsing text, UI lists | Ch. 9, 14 to 16 |
| 2 | Expandable variable tree | Easy | GDB/MI variable objects, recursive UI | Ch. 13, Step 12 |
| 3 | Test runner with gutter icons | Medium | CTest, parsing results, Monaco decorations | Ch. 12 |
| 4 | Package NOVA as an installer | Medium | electron-builder, signing, auto-update | Ch. 9 |
| 5 | Plug-in system | Medium | API design, security, loading code | Ch. 8, 9 |
| 6 | Sanitizer and memory-leak viewer | Medium | AddressSanitizer output, markers | Ch. 10, 11 |
| 7 | Profiler and coverage view | Medium/Hard | gcov, perf data, charts | Ch. 11, 12 |
| 8 | Your own language server | Hard | LSP in depth, parsing | Ch. 13 |
| 9 | A toy programming language | Hard | Lexer, parser, interpreter, compiler | Ch. 3, 8 |
| 10 | Syntax colouring with a real parser | Hard | Grammars, incremental parsing | Ch. 8, 13 |
| 11 | Remote development over SSH | Hard | Networking, security | Ch. 9 |
| 12 | Move NOVA to TypeScript and a bundler | Medium | Types, build tools | Ch. 8 |

## Project 1: A Git panel

**What.** A new bottom tab that shows which files changed, a diff, and a box to commit.

**Why.** Almost every project lives in Git. Having it inside your editor removes a constant window switch.

**First steps.**

1. Run `git status --porcelain=v1` with the existing `execCapture(cmd, args, cwd)` helper. The output is stable and easy to parse: two status letters, a space, then the path.
2. Parse each line into `{ status, path }`. Show them in a list with icons for *modified*, *added*, *untracked*, *deleted*.
3. Clicking a file runs `git diff -- path` and shows the text in the panel, colouring lines that start with `+` green and `-` red (use the same `out()` helper with a class).
4. Add a message box and a *Commit* button that runs `git add -A` then `git commit -m "message"`.
5. Re-run the status after every save and commit.
6. Bonus: show a coloured stripe in the editor margin on changed lines with Monaco decorations, using `git diff -U0` to learn the changed line numbers.

::: why Why start here?
It uses only things you already built: a process runner, text parsing, a panel and a list. It is a "same ingredients, new dish" project, perfect for confidence.
:::

## Project 2: An expandable variable tree

**What.** In the debugger, click an arrow next to a struct or array to see its members, to any depth.

**Why.** Real programs are full of structs, vectors and pointers. A single line of text such as `{ _M_impl = { _M_start = 0x1f40...` is hard to read.

**First steps.**

1. Use GDB's **variable objects**: `-var-create - * myvar` creates one for the expression; `-var-list-children name` returns its children; `-var-update *` tells you what changed.
2. Replace `-stack-list-variables --all-values` with `--simple-values`, which gives `name` and `type` and a `value` only for simple things.
3. For each variable create a var object (`-var-create`), keep its name, and show a triangle if `numchild` is greater than zero.
4. When the user clicks the triangle, call `-var-list-children` and insert child rows below, indented. Cache them.
5. After each step, call `-var-update` to refresh values and highlight those that changed.
6. Delete the var objects (`-var-delete`) when the frame changes.

## Project 3: A test runner

**What.** Little green and red marks beside each test; a panel with results; one key to run the failing tests again.

**First steps.**

1. Use CMake's `enable_testing()` and `add_test()` (Chapter 12). Run `ctest --output-on-failure` in the build folder.
2. Add `--output-junit results.xml` (or `-T Test` parsing) and read the result. Start simply: parse the lines `Test #1: area_test ... Passed`.
3. Show the list in a new panel with a tick or cross.
4. For GoogleTest or Catch2 projects, learn their `--gtest_list_tests` option to list the tests, then run one with `--gtest_filter=Name`.
5. Use Monaco decorations to place a green or red icon in the margin next to each `TEST(...)` line.

## Project 4: Package NOVA as an installer

**What.** A single `NOVA-Setup.exe` anyone can double-click, without installing Node.

**First steps.**

1. `npm install --save-dev electron-builder`.
2. Add a `"build"` block to `package.json` with `appId`, `productName`, the files to include and the Windows target (`nsis`).
3. Run `npx electron-builder --win`. The installer appears in a `dist` folder.
4. Remember that `node-pty` is a native module: electron-builder rebuilds it for the right Electron version automatically (check that the log says so).
5. Add an application icon (`.ico`), then explore code signing (to avoid Windows warnings) and `electron-updater` for automatic updates.

::: warn Watch out
The tools NOVA drives (g++, cmake, gdb, clangd) are **not** inside the installer. Show a friendly *Toolchain Check* screen at first start (you already detect them) and link to install help.
:::

## Project 5: A plug-in system

**What.** Let anyone add a feature by dropping a folder with a script into `plugins/`.

**First steps.**

1. Define the **API** you will allow: a small object with functions such as `registerAction(name, fn)`, `getActiveFile()`, `showMessage(text)` and `onSave(callback)`. Fewer functions are safer and easier to keep stable.
2. At start-up, list the `plugins` folder from the main process, read each `plugin.json` (name, version, entry file) and send the script text to the window.
3. Run the script inside a function that receives only your API object: `new Function('nova', code)(api)`.
4. Catch every exception from plug-in code (`try/catch`) so a bad plug-in cannot crash NOVA.
5. Add a *Plugins* list in Settings with an on/off switch for each.

::: why Why is this safe only in small steps?
A plug-in is code from someone else running inside your program. Start by trusting only plug-ins that *you* wrote, give them the smallest API, and only then think about sandboxing (for example running them in a hidden window or a Worker with limited access). Security is a ladder, not a switch.
:::

## Project 6: A sanitizer viewer

**What.** Compile with `-fsanitize=address,undefined` and show bugs (use-after-free, buffer overflow, leaks) as clickable problems.

**First steps.**

1. Add a **Run with Sanitizers** action that adds `-fsanitize=address,undefined -fno-omit-frame-pointer -g` to the compile line (and to linking).
2. Run the program; capture the **stderr** text. AddressSanitizer prints reports starting with `==ERROR: AddressSanitizer:` followed by a stack of lines like `#0 0x... in main main.cpp:12`.
3. Reuse the file-and-line link regex from Chapter 8 to make each frame clickable.
4. Turn the first stack frame in your code into a **marker** in the editor, as you do for compiler errors.

## Project 7: Profiler and coverage

**What.** See which lines run most (profiling) and which never run (coverage).

**First steps.**

1. Coverage with GCC: compile with `--coverage`, run the program, then run `gcov main.cpp`. It writes `main.cpp.gcov`, a copy of your source with a count at the start of each line.
2. Parse the counts: `-` means "no code", `#####` means "never run". Show those lines with a red margin mark and executed lines with green (decorations again).
3. For a profile, use `gprof` (compile with `-pg`) or a sampling tool, and draw a bar chart of time per function using SVG (Chapter 6).

## Project 8: Your own language server

You already designed it in the Chapter 13 big assignment. To grow it:

1. Write a real **lexer** and **parser** for a small language (maybe a configuration language or your toy language from Project 9).
2. Keep the AST for each open document; when `didChange` arrives, re-parse.
3. Report **syntax errors** with accurate ranges as diagnostics.
4. Implement `textDocument/definition` using a **symbol table** (a Map from names to where they were declared).
5. Connect it to NOVA by changing `lspBoot` to start your server for your file extension.

## Project 9: A toy programming language

This is the project the folder name hinted at, "C Interpreter". Writing a language is the best way to understand every language.

**First steps.**

1. Start with the calculator interpreter from Chapter 8.
2. Add **variables** (`x = 5`) with an environment (a Map).
3. Add **statements**: `print`, `if`, `while`, blocks.
4. Add **functions** and **recursion**, with a call stack of environments.
5. Add **strings** and a few built-in functions.
6. Then rewrite the evaluator as a **compiler to bytecode** plus a small virtual machine. Measure how much faster it is.
7. Finally, give your language **syntax colouring in NOVA** (a Monaco *Monarch* tokenizer, like the one NOVA already has for CMake) and a **Run** action that calls your interpreter.

::: story A path of real books
*Crafting Interpreters* by Robert Nystrom (free to read online) walks through exactly this project, step by step, first as a tree-walking interpreter, then as a bytecode virtual machine in C. After building NOVA you are ready for it.
:::

## Project 10: A real parser for colouring

**What.** Replace regular-expression-style colouring with a true grammar, as modern editors do with **Tree-sitter**.

**First steps.** Learn what a **grammar** is; run Tree-sitter's C++ grammar over a file from a small Node script and print its syntax tree; then use the tree to produce Monaco's **semantic tokens** so that, for instance, a function name and a variable name get different colours even though both are identifiers. clangd can also provide semantic tokens over LSP.

## Project 11: Remote development

**What.** Edit and build code that lives on another computer.

**First steps.** Use SSH to run commands remotely (the `ssh` program, or Node's `ssh2` library), replace `fs:read` and `fs:write` with versions that go over SSH or SFTP, and start `clangd` and `gdbserver` on the remote machine. Study how VS Code Remote and JetBrains Gateway divide work between a thin local UI and a remote "backend". It is a big project: security and network failure are the real topics.

## Project 12: Grow up: TypeScript and a bundler

NOVA is plain JavaScript on purpose: nothing to install, nothing between you and the code. As a project grows to ten thousand lines, **TypeScript** (JavaScript with types) catches whole families of mistakes before you run the program, and a **bundler** (such as esbuild) combines files and enables real `import` statements.

**First steps.** Rename `lsp.js` to `lsp.ts`. Add `npm install --save-dev typescript`. Describe the shapes you use (`interface Tab { path: string; dirty: boolean; ... }`). Let the compiler list every place where the shape is misused. Then add esbuild to produce the files the window loads.

## How to learn from here

* **Read other people's code.** Open the source of a small, well-loved tool and trace one feature from the click to the result, as you did with Run in this book.
* **Rewrite something small three times.** The first version teaches you the problem. The second shows you what you misunderstood. The third is good.
* **Keep a "bugs I have met" file.** In this book you saw several real ones: the `define` clash, the `hidden` attribute, the path-casing duplicate, the TDZ race. Each bug you remember is a shield for life.
* **Teach it.** Explain a chapter to a friend. Where you stumble is what you have not understood yet.

### Recommended reading

| Topic | Title | Why |
|---|---|---|
| C | *The C Programming Language* (Kernighan and Ritchie) | The classic; short and precise |
| C++ | *A Tour of C++* (Bjarne Stroustrup) | The creator's concise guide to modern C++ |
| C++ | *Effective Modern C++* (Scott Meyers) | The "why" behind good habits |
| JavaScript | *Eloquent JavaScript* (Marijn Haverbeke), free online | Friendly and deep |
| Web | MDN Web Docs (developer.mozilla.org) | The best reference for HTML, CSS and JS |
| C++ reference | cppreference.com | Every function, with examples |
| CMake | *Professional CMake* (Craig Scott) | The modern way to write CMake |
| Interpreters | *Crafting Interpreters* (Robert Nystrom), free online | Build a language from scratch |
| Debugging | The GDB manual and the GDB/MI chapter | Every command we used |
| Protocols | The LSP specification (microsoft.github.io/language-server-protocol) | The full list of messages |

::: quiz
1. Why is the Git panel a good first extension project?
2. Which GDB/MI commands create and fetch the children of a variable object?
3. Why should a plug-in system start with a tiny API?
4. What flags make GCC produce coverage data?
5. Why is the toy language project a good way to understand every language?
6. Name two benefits of moving a big JavaScript project to TypeScript.
:::

::: answers
1. It reuses only skills already built: running a tool, parsing its text, and showing a list in a panel.
2. `-var-create` to make it and `-var-list-children` to get its children.
3. A plug-in is someone else's code running in your program; a small API limits what can go wrong and is easier to keep stable.
4. `--coverage` (then run the program and use `gcov`).
5. You must implement lexing, parsing, scope, calls and memory yourself, which shows how every language works inside.
6. Types catch mistakes before running and make large code bases easier to change safely; also better editor help.
:::
