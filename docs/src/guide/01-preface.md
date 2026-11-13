@@unnumbered
# Preface: Read This First

## What this guide is

This guide explains one program from the inside: the **Vibration Lab**. The Vibration Lab lets you draw a system made of **masses**, **springs**, **dampers** and **ground anchors** on a canvas, press **Run**, and watch it move. It shows you the numbers while it moves (the *telemetry*), and it can record everything to an **.mp4 video** file.

Three things sit underneath the pictures, and this guide explains each one fully:

1. **The physics engine.** A small piece of C++ that knows the laws of motion. It is built with a method called the **Lagrangian approach**. You will see where every equation comes from, and exactly which line of code each equation became.
2. **The interface.** The windows, buttons, sliders, mouse handling and drawing. It is plain C++ talking to Windows. No game engine and no big library.
3. **The video recorder.** A "direct to video" encoder that takes the picture on screen, 60 times a second, and writes it into an H.264 video file, using the video encoder that is already inside Windows.

{{fig:gl-product|The whole program in one picture: what you do, what the computer does with it, and where the results go.}}

## Who this guide is for

You do not need to know advanced mathematics. If you know what a *function* is, and you have seen a little C or C++, you can follow every step. Every strange word is explained the first time it appears, and all of them are collected again in the glossary at the back (Appendix A).

If you want the long, slow introduction to C++, the other book in this series, *How to Build the NOVA C-IDE*, has full chapters on C and C++. This guide is the deep dive into one project.

## How the guide is organised

| Part | What is in it |
|------|---------------|
| I. The Idea | What we built, and the physics: Newton's way versus Lagrange's way, the derivation of our equations, and how a computer solves them (RK4). |
| II. The Engine, Line by Line | The files <code>engine.hpp</code> and <code>model_io.hpp</code>, every function, every line, each tied back to an equation. |
| III. The Interface, Line by Line | How a Windows program works, then <code>app.hpp</code>, <code>render.hpp</code> and <code>live.cpp</code>. |
| IV. The Recorder and the Tests | How video works, the Media Foundation encoder in <code>encoder.hpp</code>, and the nine self-tests that prove the program is right. |
| V. Using It and Growing It | A step-by-step user manual with worked experiments, and a list of projects to build next. |
| Appendices | Glossary, a table that links every symbol in the maths to a name in the code, and a quick-reference of keys. |

## The special boxes

::: why
A **WHY?** box explains the reason behind a choice. Knowing *why* a line is written the way it is lets you change it safely.
:::

::: whatif
A **WHAT IF** box shows what would break if the code were different. Seeing the failure makes the correct version easy to remember.
:::

::: mapx
A **MATHS TO CODE** box places an equation next to the line of code that carries it out. These are the most important boxes in Part II.
:::

::: def
A **KEY WORD** box defines a piece of jargon in plain English.
:::

::: try
A **TRY IT** box asks you to do something in the program and watch the result.
:::

::: check
A **CHECK** box says what you should see. If you do not see it, stop and find out why.
:::

## About the code listings

Every listing in this guide starts with its file name and the range of line numbers it covers, and each line carries its own number on the left. The listings are copied **automatically from the real files** when the PDF is built. That means they can never be out of date, and when the text says "line 142" it means line 142 of the real file.

::: note
The program is for Windows 10 or 11. It needs only a C++17 compiler (g++ from MSYS2 was used) and CMake. The libraries it uses (<code>user32</code>, <code>gdi32</code>, <code>gdiplus</code>, <code>mfplat</code>, <code>mfreadwrite</code>) are part of Windows.
:::

## An honest word

Reading explains; **doing** teaches. The best way to use this guide is to keep the program open beside it, change one thing at a time, and watch what happens. Every chapter ends with checks and exercises for exactly that reason.

Welcome to the Vibration Lab.
