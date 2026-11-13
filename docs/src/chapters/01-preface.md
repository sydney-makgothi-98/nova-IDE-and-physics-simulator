@@unnumbered
# Preface: Read This First

## Who this book is for

This book is for you if you have ever looked at a program like CLion, Visual Studio or VS Code and thought: *"How does this thing actually work? Could I build one?"*

You do not need to be an expert. You do not need a degree. If you can use a computer, type, and stay curious, you can follow this book. Every hard word is explained the first time it shows up. Every big idea gets a picture. And every important line of code gets a **why**: why it was written that way, and what would go wrong if it were not.

By the end you will have built, understood and be able to change a real programming tool called **NOVA IDE**. NOVA is a free, in-house program for writing, building, running and debugging **C** and **C++** code. It has a code editor, a file tree, a build button, a debugger, a terminal and a "smart helper" that suggests code and points out mistakes.

## What you will learn

You will learn two things at the same time:

1. **How an IDE is put together.** What parts it has, what each part does, and why it matters.
2. **The languages used to build it.** HTML, CSS, JavaScript, Node.js, Electron, C, C++ and CMake. Each language gets its own chapter that starts from zero and ends with a big project.

{{fig:lang-map|The six "body parts" of NOVA and the language that builds each one.}}

## How the book is organised

| Part | What is inside | Chapters |
|------|----------------|----------|
| I. Understanding IDEs | What an IDE is, its parts, how code becomes a running program, how NOVA is designed | 1 to 5 |
| II. The Languages | One chapter per language, beginner to advanced, with exercises | 6 to 13 |
| III. Building NOVA | A guided, step-by-step build of the whole IDE, file by file | 14 to 16 |
| IV. Going Further | Bigger projects you can build next | 17 |
| Appendices | A word list (nomenclature), shortcuts, and troubleshooting | A to C |

You can read straight through. Or, if you already know a language, skip its chapter. But do not skip Part I: it gives you the map for everything else.

## The special boxes

Look out for these boxes. They are the heart of the book.

::: why
A **WHY?** box explains the reason behind something. Programmers who only know *what* to type get stuck. Programmers who know *why* can fix problems and invent new things. Read every one.
:::

::: whatif
A **WHAT IF** box shows what would break if we did it another way. Seeing the failure makes the correct way stick in your memory.
:::

::: def
A **KEY WORD** box defines a piece of jargon in plain English. All of them are collected again in the Nomenclature (Appendix A).
:::

::: try
A **TRY IT** box asks you to type something and watch what happens. Typing it yourself teaches you ten times more than reading it.
:::

::: assign
A **MINI ASSIGNMENT** is a small exercise (5 to 20 minutes) that uses what you just learned.
:::

::: big
A **BIG ASSIGNMENT** is a larger project at the end of each language chapter. It combines everything in the chapter. It is meant to take a few hours and to make you proud.
:::

::: quiz
A **QUIZ** ends each chapter. Answer on paper first, then check the answers that follow it.
:::

::: check
A **CHECKPOINT** in Part III tells you what you should see on screen at that moment. If your screen looks different, stop and fix it before moving on.
:::

## What you need on your computer

To follow along you need a Windows, Mac or Linux computer and these free tools. Chapter 14 shows how to install each one.

| Tool | What it is for |
|------|----------------|
| Node.js | Runs JavaScript outside the browser. NOVA's engine runs on it. |
| A C/C++ compiler (g++ and gcc) | Turns your C and C++ code into programs. |
| CMake | Describes how to build bigger C and C++ projects. |
| gdb | The debugger: lets you pause a program and look inside it. |
| clangd | The "smart helper" that powers suggestions and error hints. |
| Any text editor | To type the code. VS Code is a fine choice for the first steps. |

## A promise, and an honest word

Here is the promise: if you read each chapter, type every example, do every assignment and take every quiz, you will be able to read, change and extend NOVA, and you will have a solid base in every language it uses.

And here is the honest word: nobody masters a language by reading alone. You become good by **writing code, making mistakes and fixing them**. This book gives you the path, the maps and the exercises. The walking is up to you.

## How to study

* **Read a little, type a lot.** After each new idea, type the example. Change a number. Break it on purpose.
* **Do the quiz before you look at the answers.** Struggling for an answer is how the memory forms.
* **Do not rush the "why" boxes.** They are the part you will still remember in a year.
* **Keep a notebook.** Write down words you do not know. Then look them up in Appendix A.
* **When stuck, read the error message slowly.** Most error messages tell you the file, the line and the problem. NOVA shows them in the Problems panel for exactly this reason.

Welcome to Team NOVA. Let's build something.
