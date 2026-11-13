@@part Part I — Understanding IDEs

# What Is an IDE?

## The workshop idea

Imagine you want to build a wooden chair. You could do it with a hand saw, a hammer, a ruler and a pencil, each kept in a different shed across the garden. Or you could do it in a workshop where the saw, hammer, ruler and pencil are on one bench, the lights are good, and a helper hands you the right tool at the right moment.

An **IDE** is that workshop, but for writing programs.

::: def IDE
**IDE** stands for **Integrated Development Environment**. *Integrated* means "joined together into one". *Development* means "making software". *Environment* means "the place where you work". So an IDE is one place where all the tools for making software are joined together.
:::

You can write a program in the simplest text editor, such as Notepad. Programs have been written that way for decades. But then you must save the file, open a separate window, type a long command to turn your code into a program, run it, and read the errors in yet another window. Each step is small. Together they are slow and tiring.

An IDE puts it all in one window. You type your code, press one green button, and the program runs.

{{fig:ide-vs-editor|A text editor is the centre of the onion. An IDE wraps more and more helpful layers around it.}}

## Editor versus IDE

Many people mix these two up, so let's be clear.

| | Text editor | IDE |
|---|---|---|
| Typing and saving code | Yes | Yes |
| Colouring words (syntax highlighting) | Often | Yes |
| Press one button to build and run | No | Yes |
| Understands your code (suggests, finds errors) | No | Yes |
| Debugger built in | No | Yes |
| Terminal built in | Rarely | Yes |
| Starts fast, small | Yes | Heavier |

::: story In plain words
A text editor is a **pencil**. An IDE is a **pencil, a ruler, an eraser, a calculator and a teacher looking over your shoulder**, all in one.
:::

## A short history

* **1950s to 1970s.** Programmers wrote code on punch cards. There was no screen. You handed the cards in and came back hours later to see if it worked.
* **1980s.** Programs like Turbo Pascal (1983) put an editor and a compiler into one program. You could write, run and fix code in seconds. This was the first popular IDE.
* **1990s.** Visual Studio and Delphi added drag-and-drop windows, debuggers and project management.
* **2000s.** Eclipse and IntelliJ IDEA added *code understanding*: the IDE learned to suggest the next word and rename things safely.
* **2010s to today.** JetBrains made tools for each language. **CLion** is their IDE for C and C++. Microsoft's VS Code popularised the idea of a lightweight editor extended with plug-ins and a **language server** (you will meet this in Chapter 2).

## What CLion does, and why we built NOVA

CLion is a professional IDE for C and C++. It has a project tree, a smart editor, CMake support, a debugger, a terminal and a dark, modern look. It is excellent. But it is paid software, and it is closed: nobody outside JetBrains can see how it is built.

Team NOVA decided to build **our own** IDE with the same *workflow*: the same layout, the same shortcuts, the same ideas. The goals were:

1. **Free to use** inside the team, forever.
2. **Understandable.** Every file small enough to read in one sitting.
3. **A teaching tool.** If you build an IDE, you learn how all your other tools work.

::: why Why build your own IDE at all?
Because an IDE touches almost every big idea in programming: files, processes, text, windows, networks of messages, compilers and debuggers. Building one is like building an engine to learn how cars work. You will not beat a professional car factory. But you will never again be confused by what is under the bonnet.
:::

::: warn Watch out
NOVA is **not** a copy of CLion's code. CLion's program files are private and protected by law. NOVA was designed from public documentation, common knowledge and the team's own ideas, and it uses open-source building blocks (Monaco, Electron, xterm.js, clangd, gdb). This is both legal and a better way to learn.
:::

## A picture of NOVA

Here is NOVA running. Do not worry about the details yet. By the end of the book you will know what every pixel is for.

{{img:shot-welcome.png|NOVA's welcome screen: start a new project, open a folder, or pick a recent project.}}

{{img:shot-run.png|NOVA with a project open. The file tree is on the left, the code in the middle, and the Run panel at the bottom shows the program's output.}}

::: quiz
1. What does the word "integrated" mean in IDE?
2. Name three things an IDE does that a plain text editor usually does not.
3. Which famous IDE from JetBrains is for C and C++?
4. Give two reasons Team NOVA built its own IDE.
5. Is NOVA built from CLion's source code? Explain.
:::

::: answers
1. Joined together into one. All tools live in one program.
2. Any three of: build and run with one button, suggest code as you type, find mistakes while you type, debugger, built-in terminal, project tree.
3. CLion.
4. Free to use, easy to understand, and a great way to learn how developer tools work.
5. No. CLion's code is private. NOVA was designed from public ideas and open-source parts.
:::

# The Parts of an IDE

An IDE is a team of small helpers. Each helper does one job well. When you understand the helpers, no IDE will ever look mysterious again. Let's meet them one by one. For each we answer three questions: **What is it? Why does it matter? What happens if it is missing?**

{{fig:ide-anatomy|The seven regions of an IDE window. The numbers match the sections below.}}

## The code editor

**What it is.** The big area where you type code. A good code editor does more than store letters. It colours keywords, matches brackets, lets you fold blocks, finds and replaces text, and handles huge files without slowing down.

**Why it matters.** You will look at this area for 95 percent of your working day. If it is slow, ugly or clumsy, everything feels bad.

**If it were missing.** You would have nothing to type into. There is no IDE without an editor.

::: def Syntax highlighting
Colouring different kinds of words differently (for example keywords orange, strings green, numbers blue). It lets your eyes see the *structure* of the code instantly. A missing quote mark shows up because everything after it turns green.
:::

NOVA does not build an editor from nothing. That would take years. It uses **Monaco**, the open-source editor that powers VS Code. We add our own colours, snippets and features on top.

## The project tree

**What it is.** The panel on the left that shows your folders and files as a tree you can open and close.

**Why it matters.** Real programs have tens or thousands of files. You need a map. The tree is that map. It also lets you create, rename and delete files without leaving the IDE.

**If it were missing.** You would have to open files by typing their names, and you would lose track of how your project is organised.

## Tabs

**What they are.** Each open file gets a tab at the top, like pages in a web browser.

**Why they matter.** You constantly jump between a few files. Tabs make that one click. A small dot on a tab warns you that the file has unsaved changes.

## The build system

**What it is.** The part that turns your code into a program you can run. It starts the **compiler** and the **linker** for you, with the right options, and reports problems.

**Why it matters.** Building a big C++ program by hand needs a very long command. Get one option wrong and it fails. The build system remembers the recipe so that you press one button.

**If it were missing.** You would copy and paste long commands into a terminal every time you changed one line.

NOVA's build system uses **CMake** to describe the recipe, **Ninja** to follow it quickly, and **g++** (the compiler) to do the real translation. Chapter 3 explains each.

## The run console

**What it is.** A panel at the bottom that shows what your program prints and lets you type input back to it.

**Why it matters.** Programs talk to you through text. If you cannot see it, you cannot test your work.

## The problems panel and error squiggles

**What it is.** A list of mistakes and warnings, plus red or yellow wavy lines under the exact words that are wrong.

**Why it matters.** Mistakes are normal. What matters is how fast you find them. A wavy line under the word is far faster than reading line 4,287 of a log.

{{img:shot-problems.png|Errors and warnings in NOVA. The wavy lines in the editor and the entries in the Problems panel come from two different sources: the compiler and the language server.}}

## The language server (code intelligence)

**What it is.** A separate program that actually *understands* your C++. When you type `std::vec` it knows you might want `vector`. When you hover over a function it shows its documentation. When you press a key on a name it can jump to where it was defined.

**Why it matters.** This is the biggest difference between a plain editor and an IDE. It makes you faster and catches errors before you even build.

**If it were missing.** You could still write code, but you would type everything yourself and find every mistake only after building.

::: def Language server and LSP
A **language server** is a program that understands one programming language. **LSP** (the **Language Server Protocol**) is the agreed set of messages an editor and a language server use to talk. Because the messages are standard, one server (like **clangd** for C and C++) works in many editors. NOVA uses clangd.
:::

{{img:shot-completion.png|Auto-completion in action. NOVA asked clangd what could follow "std::vec" and clangd answered with a list.}}

## The debugger

**What it is.** A tool that lets you **pause** a running program at a chosen line, look at the values of its variables, and move forward one step at a time.

**Why it matters.** Printing messages to find a bug is like searching a dark room with a match. A debugger turns on the light.

**If it were missing.** You would guess. Programmers have lost days of their lives to guessing.

::: def Breakpoint
A **breakpoint** is a mark on a line that says "stop here when you get to it". It shows as a red dot in the margin.
:::

{{img:shot-debug.png|NOVA paused at a breakpoint on line 9. The arrow marks the current line; below are the call stack, the variables and the watch list.}}

## The terminal

**What it is.** A real command line inside the IDE.

**Why it matters.** Some jobs are easiest from the command line: running git, listing files, trying a command. Having it inside the IDE means you never switch windows.

## Search and navigation

**What it is.** Tools to find things fast: find in the current file, find across the whole project, jump to a file by typing a few letters, jump to a function by name.

**Why it matters.** In a big project, *finding* code takes more time than *writing* it. NOVA copies JetBrains' famous **double-Shift** "Search Everywhere".

{{img:shot-search.png|Search Everywhere. Typing "sha" finds files, symbols and actions whose names contain those letters in order.}}

## The status bar and settings

**What they are.** A strip at the bottom with small facts (line and column, which compiler, which language), and a settings window for fonts, themes and options.

**Why they matter.** Small facts prevent big confusion. You always know which compiler is in use and where your cursor is.

## All the parts together

The magic of an IDE is not any single part. It is that all the parts **share information**. The compiler's error appears as a wavy line in the editor. The debugger's current line is highlighted in the editor. The tree opens files in the editor. Everything is connected.

::: quiz
1. Which part of an IDE shows a map of your files?
2. What is a breakpoint?
3. What does the language server do that the compiler does not?
4. Why does the status bar matter, even though it is small?
5. Name the open-source editor NOVA is built on.
6. Why is "integrated" the most important word in IDE?
:::

::: answers
1. The project tree.
2. A marker on a line telling the debugger to pause the program when it reaches that line.
3. It understands your code while you type: it suggests code, shows documentation, finds errors early and jumps to definitions. The compiler only runs when you build.
4. It tells you facts (cursor position, compiler, language) at a glance, preventing confusion.
5. Monaco (the editor of VS Code).
6. Because the value comes from the parts sharing information, for example an error found by the compiler appearing as a wavy line in the editor.
:::

# From Code to a Running Program

To build an IDE for C and C++ you must understand what happens between "I typed some code" and "the program runs". This chapter explains it in simple steps. It also answers a common question: *what is an interpreter, and is NOVA one?*

## Your computer only speaks one language

A computer's brain, the **CPU**, understands only **machine code**: long lists of numbers. Nobody wants to write that. So humans invented **programming languages** like C++, which are readable. A special program must translate our readable code into machine code. There are two ways to do the translation.

## Compilers and interpreters

{{fig:interp-vs-comp|Three ways of turning code into action.}}

::: def Compiler
A **compiler** translates your *whole* program into a separate machine-code file **once**, before it runs. Afterwards you run the file as many times as you like, very fast. **C and C++ use compilers.**
:::

::: def Interpreter
An **interpreter** reads your program and carries out the instructions **as it goes**, one by one, with no separate file. Starting is instant, but running is usually slower. **Python and (originally) JavaScript use interpreters.**
:::

Think of a speech. A **compiler** is a translator who takes your whole written speech home, translates it carefully, and gives you a finished translated copy. An **interpreter** is a translator who stands beside you and translates every sentence live as you speak.

::: why Why does this matter for an IDE?
The folder NOVA lives in is called "C Interpreter" but NOVA is not an interpreter. It is an IDE that **drives a compiler**. This matters because it decides what the IDE must do: when you press Run, it must start the compiler, wait, check for errors, then start the new program. An interpreter-based IDE would simply feed your code to the interpreter. (JavaScript, which NOVA itself is written in, is actually a *hybrid*: modern browsers interpret first and compile the busy parts. See the figure.)
:::

## The four steps inside a C++ compiler

When you type `g++ main.cpp -o hello.exe`, four stages happen quietly. You must know them because every error you will ever see comes from one of them.

{{fig:compile-pipeline|The four stages of turning source code into a program.}}

1. **Preprocessing.** Lines that begin with `#`, like `#include <iostream>`, are handled first. `#include` simply pastes the contents of another file in. After this step, your file is one very long piece of text.
2. **Compiling.** The compiler checks the grammar of your code and translates it into **assembly**, a low-level readable form of machine code. *Syntax errors* ("missing semicolon") come from here.
3. **Assembling.** The assembler turns assembly into **object code** (`main.o`): real machine code, but with gaps where it needs to find other pieces.
4. **Linking.** The **linker** joins your object files and libraries into one finished program, filling in the gaps. *"Undefined reference"* errors come from here.

::: whatif What if there were no linker?
Large programs are written in many files. Each file is compiled on its own, so each has gaps where it calls functions defined elsewhere. Without a linker, nothing would join the files. You would be forced to write all your code in a single file. The linker is what makes big projects possible.
:::

## How does an interpreter really work?

Even though NOVA is not an interpreter, you should know how one works. It is a famous exercise and you will build a tiny one in the JavaScript chapter. An interpreter has three stages.

{{fig:interp-stages|The stages of a tiny interpreter, shown on the sum "1 + 2 * 3".}}

1. **Lexer** (or tokenizer). It cuts the text into small meaningful pieces called **tokens**: numbers, plus signs, names.
2. **Parser.** It arranges tokens into a **tree** that shows the structure, called an **AST** (Abstract Syntax Tree). The tree is how the machine knows that `2 * 3` happens *before* adding 1.
3. **Evaluator.** It walks the tree from the bottom up, calculating as it goes.

::: why Why a tree and not just a list?
Because meaning depends on **grouping**. In `1 + 2 * 3` the answer is 7, not 9, because multiplication binds tighter. A flat list of tokens cannot show grouping. A tree can. Compilers, language servers and IDE features like "rename" all use trees like this. When clangd highlights a word it has already built the tree of your file.
:::

## So where does an IDE fit?

{{fig:build-flow|What NOVA does when you press the Run button.}}

An IDE is the **manager** that calls all these workers for you. The flowchart shows what NOVA does when you press Run. Notice that NOVA never translates any code itself. It **starts other programs** (cmake, g++, your program), **listens** to what they print, and **shows** the result nicely. That is the central idea of this whole book:

> **An IDE is a friendly control panel for other programs.**

::: quiz
1. What is the difference between a compiler and an interpreter?
2. Is NOVA an interpreter? What is it?
3. Name the four stages inside a C++ compiler run.
4. Which stage produces "undefined reference" errors?
5. What does a lexer produce?
6. Why do interpreters and compilers build a tree?
7. In one sentence, what does an IDE do when you press Run?
:::

::: answers
1. A compiler translates the whole program first into a separate file; an interpreter carries out the code as it reads it.
2. No. NOVA is an IDE that drives a compiler (g++) and other tools.
3. Preprocessing, compiling, assembling, linking.
4. Linking.
5. Tokens (small meaningful pieces such as numbers, names and operators).
6. A tree shows grouping and order of operations, which a flat list cannot.
7. It saves files, starts the build tools, starts the program, shows its output and shows any errors in the editor.
:::

# How NOVA Is Designed

Now we look at the **blueprint** of NOVA. Do not worry if some words are new; each is explained here and again in the language chapters.

## Two kinds of processes

NOVA is built with **Electron**. Electron is a toolkit that lets you build desktop programs out of web technology (HTML, CSS and JavaScript). Chrome and VS Code are built this way.

An Electron program is split into two kinds of **processes**. A **process** is a running program with its own private memory.

* The **main process** (our file `main.js`) is the *engine room*. It runs on **Node.js**, so it is allowed to touch files, start other programs and open windows.
* The **renderer process** (our files in the `renderer` folder) is the *window you see*. It is a web page, written in HTML, CSS and JavaScript. For safety it is *not* allowed to touch your disk or start programs.

Between them sits a thin **bridge** (`preload.js`) that decides exactly which doors are open.

{{fig:architecture|NOVA's architecture. The window at the top never touches the computer directly. Everything passes through the bridge to the main process, which starts the other programs.}}

::: why Why split it in two?
**Safety and stability.** A web page can show content from anywhere. If it had full power over your computer, one bad page could delete your files. Splitting the powers means the window can only do what the bridge allows. It also means that if the window freezes, the engine room keeps running.
:::

::: whatif What if the window could touch the files directly?
It would work, and it would be simpler to write. But any script that sneaked into the window (for example from a malicious file you opened) could read, change or delete anything on your computer. The two-process design closes that door. Electron's own guidelines recommend it.
:::

## How the two sides talk: IPC

The window and the engine room are separate programs, so they talk by sending messages. This is **IPC**, **Inter-Process Communication**.

{{fig:ipc-sequence|One IPC round trip: the window asks for a file and gets its text back.}}

In NOVA you will see lines like `await nova.read(path)`. That one line travels through the bridge to the main process, which reads the file from disk, and the answer travels back. The word `await` means "wait here until the answer arrives, but keep the window alive".

## NOVA also talks to other programs

The main process does not only read files. It starts helper programs and **pipes** text to and from them. A **pipe** is a one-way channel for text between two programs, like a tube.

| Helper program | What NOVA uses it for | How NOVA talks to it |
|---|---|---|
| g++ / gcc | Compile code | Command-line arguments; reads its output |
| cmake and ninja | Describe and perform builds | Command-line arguments; reads output |
| your program | Run what you built | Reads its output, can send input |
| clangd | Code intelligence | JSON messages (LSP) over pipes |
| gdb | Debugging | Text commands (GDB/MI) over pipes |
| PowerShell (via a PTY) | The terminal | A pseudo-terminal |

{{fig:lsp-flow|A conversation between NOVA and the language server clangd.}}

{{fig:gdb-flow|A conversation between NOVA and the debugger gdb.}}

## The folder map

{{fig:folder-tree|Every file in NOVA with a one-line description.}}

## How big is NOVA?

NOVA is small enough for one person to read in a weekend. This chart shows how many lines each file has.

{{fig:loc-chart|Lines of code per file. app.js is the largest because it holds most of the window's logic.}}

::: why Why keep files small and separate?
Each file does **one job**. `lsp.js` knows only how to talk to clangd. `debug.js` knows only how to talk to gdb. If the debugger breaks, you open one file. If you want to replace clangd with another language server, you change one file. This idea is called **separation of concerns**, and it is the most important habit of professional programmers.
:::

::: quiz
1. What is a process?
2. Which process may touch the disk: main or renderer?
3. What is the job of preload.js?
4. What does IPC stand for?
5. Why does NOVA talk to clangd with JSON messages?
6. What does "separation of concerns" mean, and why is it useful?
:::

::: answers
1. A running program with its own private memory.
2. The main process.
3. It is the safe bridge: it exposes only chosen functions (window.nova.*) from the main process to the window.
4. Inter-Process Communication.
5. LSP, the standard language-server protocol, is made of JSON messages, so one server works with many editors.
6. Each file or part does one job. It makes code easier to read, fix and replace.
:::

# The Languages Behind NOVA

NOVA is a team of languages, each with its own job. Using the right language for each job is what makes the program small.

{{fig:lang-map|Who does what. HTML is the skeleton, CSS the skin, JavaScript the brain, Node.js the engine room, C and C++ the cargo, and CMake plus JSON the glue.}}

## HTML: the skeleton

**HTML** (HyperText Markup Language) describes *what is on the screen*: a toolbar, a sidebar, a button. It is not a programming language, because it makes no decisions. It is a **markup** language: it labels things. In NOVA, `index.html` lists every panel.

## CSS: the skin

**CSS** (Cascading Style Sheets) says *how things look*: colours, sizes, spacing, fonts. NOVA's dark and light themes are just two sets of colours in `styles.css`. Change a colour in one place and the whole IDE changes.

## JavaScript: the brain

**JavaScript** is a real programming language. It makes decisions and reacts to clicks, key presses and messages. Most of NOVA's logic is JavaScript: opening tabs, running builds, talking to clangd and gdb.

::: why Why JavaScript and not C++ for the IDE itself?
You could write an IDE in C++. Many do. But JavaScript with Electron gives us a ready-made window system, text editor (Monaco), terminal (xterm.js) and fast screen drawing. We write far less code, and JavaScript lets us change something and see the result in seconds. The price is more memory use. For a team tool, that is a good trade.
:::

## Node.js and Electron: the engine room

**Node.js** is JavaScript that runs *outside* a browser, so it can read files, start programs and use the network. **Electron** bundles Node.js with a browser engine to create desktop applications. NOVA's `main.js` and `preload.js` use them.

## C and C++: the cargo

C and C++ are the languages NOVA is built **for**. They are not used to build NOVA's window, but you need to know them well to test, use and extend the IDE: you will write C and C++ programs to try every feature. They are also the language of the tools NOVA drives (gcc, g++, clangd and gdb are themselves written in C and C++).

## CMake and JSON: the glue

**CMake** is a small language for describing how to build C and C++ projects. NOVA writes CMake files for new projects and runs CMake for you. **JSON** is a simple text format for data. NOVA uses it for settings, for the compile database and for every message exchanged with clangd.

## Helpers you will meet briefly

* **GDB/MI** and **LSP**: "mini languages" of messages. We study them in Chapter 13.
* **Batch** (`Launch NOVA.bat`) and **PowerShell**: small scripting languages used to start the program.
* **Python**: not part of NOVA itself. The book's own build script uses it to turn this text into the PDF you are reading.

## The learning order

The language chapters are in the order that makes learning easiest:

1. HTML (easiest, you see results at once)
2. CSS (makes it pretty)
3. JavaScript (your first real programming)
4. Node.js and Electron (power tools)
5. C (the foundation of low-level computing)
6. C++ (C with superpowers)
7. CMake (builds)
8. JSON and protocols (how programs talk)

::: quiz
1. Which language describes what is on the screen?
2. Which describes how it looks?
3. Why is HTML called a markup language and not a programming language?
4. Why was JavaScript chosen to build the IDE window?
5. Is NOVA's window written in C++? What is C++ used for in this book?
6. What does Node.js allow JavaScript to do that a browser does not?
:::

::: answers
1. HTML.
2. CSS.
3. It only labels and structures things; it does not make decisions or calculate.
4. With Electron it gives a ready-made window, editor and terminal, needs far less code, and changes can be seen in seconds.
5. No. C and C++ are the languages that NOVA edits, builds and debugs, and the languages the helper tools are written in.
6. Read and write files, start other programs and use the network.
:::
