# Updating and Editing the Suite

This chapter is the maintainer's manual. It explains, from a clean checkout to a published release, **how a change to the Vibration Lab Suite is made, tested, built, packaged and pushed**, and what to do when a step fails. Every command here is one that was actually used to ship version 1.2, including the failures, which are described because they are the most useful part.

::: note
**Two destinations, always.** A change is only finished when it has reached **both** places: the **repository** (the source, so others can see and build it) and the **build** (the packaged program in the <code>dist</code> folder, so people can run it). Doing one without the other is the most common mistake, and the last section of this chapter shows how to check that both are current.
:::

# The Map: Where Everything Lives

The suite is one folder, <code>vlab-suite</code>, inside the larger NOVA project. The parts that matter:

| Folder or file | What it is | Edit it when |
|----------------|------------|--------------|
| <code>core/include/vlab/*.hpp</code>, <code>core/src/*.cpp</code> | **The engine.** Bodies, links, forces, constraints, contacts, analysis, saving and loading. No window, no operating-system code. | You change the physics or the file format |
| <code>app/*.cpp</code>, <code>app/app.hpp</code> | **The program you see.** The canvas, panels, plots, recording. Uses Dear ImGui. | You change the interface |
| <code>io/</code> | Excel, CSV and schematic export | You change what is exported |
| <code>tests/core_tests.cpp</code> | **The validation suite.** Every physics claim, checked against theory | Always, when you add or change physics |
| <code>tools/cli.cpp</code> | A command-line runner (no window) | Rarely |
| <code>tools/package.py</code> | **The release script.** Builds, tests and packages | You change what ships |
| <code>CMakeLists.txt</code>, <code>app/app.cmake</code> | The build description | You add a source file |
| <code>third_party/</code> | Dear ImGui and ImPlot, copied in (never edit) | You upgrade a library |
| <code>ue5/</code> | The **Unreal Engine plugin**. It contains a *copy* of the engine in <code>ue5/VLab/Source/VLabRuntime/ThirdParty</code> | Never directly: it is overwritten by the sync script |
| <code>build/</code> | Build output. **Never committed** | Never |
| <code>dist/</code> | **The released package**: the program, tools, examples, documents. Committed | By the packaging script only |
| <code>dist-1.2/</code> | A second, uncommitted copy of the same package, used as a safety copy | By the packaging script with <code>VLAB_DIST</code> |
| <code>../docs/</code> | The books (this guide is <code>docs/src/suite/*.md</code>) and the PDFs built from them | You change the documentation |

::: why
**Why is the engine separate from the program?** Because the same engine runs in the window, in the command-line tool, in the validation suite and inside the Unreal plugin. Anything that is *not* in <code>core</code> cannot be tested without a window, and a physics bug found in the window has to be reproduced there. Keeping the physics in <code>core</code> means every physics bug can be reproduced by a ten-line console program, which is how most of the bugs in this project were found (see "Reproduce before you fix").
:::

# One-Time Setup

You need five tools. All are free.

| Tool | Used for | How to check it works |
|------|----------|-----------------------|
| **MSYS2 with the mingw-w64 toolchain** (g++ 15 was used) | Compiling C++ | <code>g++ --version</code> |
| **CMake** (3.16 or newer) | Describing the build | <code>cmake --version</code> |
| **Ninja** | Running the build quickly | <code>ninja --version</code> |
| **Python 3** | The packaging script and the guide builder | <code>python --version</code> |
| **Git** | Version control | <code>git --version</code> |

For the guide (PDF) you also need **PyMuPDF** (<code>pip install pymupdf</code>) and the project's Electron (installed by <code>npm install</code> in the NOVA folder), because the PDF is rendered by printing a web page.

## The one setup trap: the compiler must be on the PATH

The compiler lives in <code>C:\msys64\mingw64\bin</code>. If that folder is not on the PATH of the terminal you are using, the build fails in a confusing way: <code>g++</code> may simply exit with an error code and print nothing at all. This happened during the development of 1.2 in one shell (a sandboxed bash) while the same command worked in PowerShell once the folder was added. The fix is always the same, and it is worth putting at the top of every PowerShell session you use for building:

```powershell
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
```

::: tip
**Test the compiler on a one-line program first.** <code>echo int main(){return 0;} > t.cpp</code> then <code>g++ t.cpp -o t.exe</code>. If *that* fails, nothing else will work and the problem is the environment, not your code.
:::

# The Everyday Loop: Edit, Build, Test

Every change, large or small, goes round the same four-step loop. Do not skip steps to save time; the steps you skip are the ones that find the bug.

1. **Edit** the source.
2. **Build**: from the <code>vlab-suite</code> folder,

```powershell
cmake --build build
```

   The first time on a new machine, configure first: <code>cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++</code>. After that <code>cmake --build build</code> compiles **only the files that changed** and relinks the four programs: the engine library, <code>vlab_cli.exe</code>, <code>vlab_tests.exe</code> and <code>vlab_suite.exe</code> (the window).
3. **Test**: <code>.\build\vlab_tests.exe</code>. It prints every check as PASS or FAIL with the measured and theoretical numbers, and ends with a line such as <code>92 of 92 checks passed</code>. The exit code is 0 only if everything passed.
4. **Look at it** in the real program: run <code>build\vlab_suite.exe</code>. A passing test proves the physics; only your own eyes prove the *click* works.

::: warn
**Passing tests do not prove the interface works.** Three of the bugs fixed in 1.2 were interface bugs that no test could see: a crash when a new connector was chosen (a list of names indexed past its end), a distribution that could not be clicked on, and a run time that kept coming back as 10 s because the saved last session restored it. Always exercise the thing you changed with the mouse.
:::

## Reading a build error

Compiler messages look hostile but follow a pattern: **file, line, column, then the problem**. Read the *first* error only; the later ones are usually consequences. A real example from this project:

```
app/ui_canvas.cpp:784:32: error: use of deleted function 'void ImDrawList::AddPolyline(...)'
```

The cause was a drawing call whose argument types matched a deliberately disabled overload in the library. The fix (drawing the curve as a series of short lines) took one edit. The habit to learn is: go to the line, read the message, change **one** thing, rebuild.

## Reproduce before you fix

When something behaves wrongly, **do not edit the engine first**. Write a small standalone program in a scratch folder that builds the model by hand and prints the numbers. Compile it against the engine with one command:

```powershell
g++ -std=c++17 -O2 -Icore/include repro.cpp (Get-ChildItem core/src/*.cpp).FullName -o repro.exe
```

Because the engine has no window code, this works for any physics question. The 1.2 collision work began exactly this way: a program that drove a block into a wall at different forces, printed where it ended up, and showed that the engine's behaviour at ordinary forces was right and that tunnelling happened only at absurd speeds. Without that program the temptation would have been to rewrite the contact solver, which was not at fault.

# Making a Change: The Full Checklist

The checklist below is the one used for every feature of 1.2 (distributions, torsional springs, moments, time-varying loads, the delete icon, the zoom buttons). Not every item applies to every change.

| # | Step | Where | Why |
|---|------|-------|-----|
| 1 | Write down the expected behaviour and, for physics, a closed-form answer | On paper | A test you can check by hand cannot be wrong in the same way as the code |
| 2 | Add data to the model with a default that means "off" | <code>core/include/vlab/model.hpp</code> | Old files and old code keep working |
| 3 | Add the behaviour to the engine | <code>core/src/world.cpp</code> (or <code>expr.cpp</code>, etc.) | The physics must be testable without a window |
| 4 | Write the test | <code>tests/core_tests.cpp</code> | The test is the specification |
| 5 | Save and load the new data | <code>core/src/project_io.cpp</code> | A feature that is lost when you save is not a feature |
| 6 | Add the interface: a tool, a panel field, how it is drawn, how it is picked, how it is deleted | <code>app/ui_panels.cpp</code>, <code>app/ui_canvas.cpp</code>, <code>app/app.hpp</code> | The user cannot use what they cannot reach |
| 7 | Build, run the validation suite, then use it in the window | | See "The everyday loop" |
| 8 | **Sync the Unreal plugin** | <code>python ue5/sync_core.py</code> | The plugin holds its own copy of the engine |
| 9 | Update the guide | <code>docs/src/suite/*.md</code> and rebuild the PDF | The documentation must match the program |
| 10 | Package | <code>python tools/package.py</code> | Produces <code>dist</code> |
| 11 | Commit and push | Git | Publishes the source |

## Save-file compatibility: the rule of appending

Files are plain text, one line per body and per link, with fields separated by tabs. The loader reads field *n* by position. The rule that has kept every old file working is therefore:

::: def
**Only ever append.** A new field goes at the **end** of the line, and the loader reads it only if the line is long enough (<code>if (f.size() &gt; 49) ...</code>), otherwise it leaves the default. Never insert a field in the middle and never reorder: every later field would shift, and old files would silently load wrong numbers.
:::

The same rule explains a subtle behaviour: a design saved with a 10 s run time keeps 10 s when reopened, because the file stored a value. Changing a *default* in the code changes only designs that have no stored value.

# Packaging: What the Release Script Does

<code>tools/package.py</code> turns the working tree into the thing people run. Run it from anywhere:

```powershell
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
python tools\package.py
```

It performs these steps in order, and stops at the first failure:

1. **Configure and build** with CMake and Ninja in Release mode (<code>-O2</code>). This always rebuilds anything stale, so the package can never contain an old binary.
2. **Run the validation suite.** If any check fails the script prints "validation suite FAILED: not packaging" and exits. **This is a gate**: a build that fails its physics tests cannot be released by accident. The suite's output is saved as <code>Documents\validation_report.txt</code> inside the package, so the report that ships is the report of the exact binary that ships.
3. **Copy the programs**: the window (<code>VibrationLabSuite.exe</code>), the command-line runner and the test runner (into <code>Tools</code>), with debug symbols stripped to save space.
4. **Write every built-in example** as a <code>.vlab</code> file into <code>Examples</code>, by asking the command-line runner to save each one. The examples therefore always match the code that defines them.
5. **Check the Excel export** on a fresh run (the structure always; the charts too if the Python library <code>openpyxl</code> is installed).
6. **Copy the documents**: the user guide PDF, the README and the third-party licences.
7. **Copy the Unreal plugin** from <code>ue5</code>.
8. **Zip everything** into <code>VibrationLabSuite-1.2-win64.zip</code>.

The last two lines it prints name the zip and the executable. If you do not see them, the package was not produced.

## The version number

The version lives in two places that must agree: <code>VERSION = "1.2"</code> near the top of <code>tools/package.py</code> (it names the zip), and the cover text of the guide. Change both when you start a new release.

## The safety copy: VLAB_DIST

By default the package is written to <code>dist</code>. Setting the environment variable <code>VLAB_DIST</code> to another folder writes it there instead:

```powershell
$env:VLAB_DIST = "$PWD\dist-1.2"
python tools\package.py
Remove-Item Env:VLAB_DIST
```

During 1.2 the script was run twice per change: once for <code>dist</code> (committed) and once for <code>dist-1.2</code> (a convenient local copy to run from while <code>dist</code> is in use). Remember to remove the variable afterwards, or the next run will go to the wrong folder.

## The failure that teaches the most: the locked executable

If **the program is running from <code>dist</code> while you package**, Windows refuses to overwrite the running executable, and the script stops with:

```
PermissionError: [Errno 13] Permission denied: '...\dist\VibrationLabSuite\VibrationLabSuite.exe'
```

The script deletes the old package folder *before* copying the new files, so a failure at this point leaves <code>dist</code> **half rebuilt**: the README and tools gone, the old executable still there (locked). This happened in 1.2, and the half-built folder was committed by mistake with <code>git add -u</code>, which stages *deletions* as well as changes. The repository briefly held a broken package. The recovery was:

```powershell
git checkout <last-good-commit> -- dist     # restore the previous complete package
git commit -m "Restore dist/ after an interrupted packaging run"
git push
```

and then, with the program closed, running the packaging script again and committing the result.

::: warn
**Before packaging, close every copy of VibrationLabSuite.** Check with <code>Get-Process | Where-Object { $_.ProcessName -like "VibrationLab*" }</code>. If nothing is listed, packaging is safe. Never kill someone else's running window to make it work; ask them to close it.
:::

# Keeping the Unreal Plugin in Step

The Unreal plugin does not link to the engine in <code>core</code>. It contains its **own copy** of the engine's source files, so that it can be built by Unreal's own build system. After *any* change to a file in <code>core/include</code> or <code>core/src</code>, run:

```powershell
python ue5\sync_core.py
```

It deletes the plugin's copy and copies the current engine in. Then commit the changed files under <code>ue5/</code> together with the engine change. If you forget, the desktop program and the Unreal plugin silently drift apart: the same model would behave differently in the two. The check is simple: after syncing, <code>git status</code> should show changes under <code>ue5/VLab/Source/VLabRuntime/ThirdParty</code> that mirror the changes under <code>core</code>.

# Git: Committing and Pushing

## The state of the repository

Work happens on a **branch** (during 1.2 the branch was <code>v1.2-deformation</code>); the stable line is <code>main</code>. The branch has a **remote** called <code>origin</code>, the copy on GitHub. Four commands tell you where you are:

| Command | Tells you |
|---------|-----------|
| <code>git status --short</code> | Which files changed (<code>M</code>), were added (<code>A</code>), deleted (<code>D</code>) or are not tracked (<code>??</code>) |
| <code>git branch --show-current</code> | Which branch you are on |
| <code>git log --oneline -5</code> | The last five commits |
| <code>git diff --stat</code> | A one-line-per-file summary of what you changed |

## What to stage, and what never to

* **Stage with <code>git add -u</code>** to include every *already tracked* file you modified or deleted (source, tests, and the files under <code>dist</code>). It never adds new files by accident, which is why it was used for 1.2.
* **Add a new file explicitly**: <code>git add path\to\file</code>. A new source file, a new test, a new chapter must be added by name, or it will not be in the commit.
* **Never commit** <code>build/</code> (it is in <code>.gitignore</code>), scratch programs, or <code>dist-1.2</code> (left untracked on purpose; <code>git status</code> shows it as <code>??</code>).
* **Read <code>git status</code> before every commit.** An unexpected <code>D</code> (deleted) line is how the half-built <code>dist</code> was nearly published.

::: why
**Why is <code>dist</code> in the repository at all?** Because the repository is also the place people download the finished program from. The cost is that every release makes large binary changes. The alternative, publishing zips as separate release downloads, keeps the repository small but needs a second step and a second place to look. The project chose one place.
:::

## Writing the commit message

A commit message is the changelog you will want in a year. The pattern used throughout 1.2: a **short first line** that says what changed in the user's terms, a blank line, then bullets for the details.

```
Spring and damper distributions (constant or linear) and torsional springs

- Spring/Damper links can be spread along a span (constant, or y = m x), N elements
- Pin joints gain a torsional spring (kt); new Torsional spring connector
- New Connections menu entries; properties panel fields; drawing; save/load
- Tests: distribution stiffness = k x span and m span^2 / 2; torsion period
```

## Committing and pushing

```powershell
git status --short                         # read it
git add -u                                 # tracked changes, including dist
git add docs\src\suite\05-new-chapter.md   # any new file, by name
git commit -m "First line" -m "Details"
git push origin v1.2-deformation
```

The last line of <code>git push</code>'s output has the form <code>old..new  branch -> branch</code>, for example <code>f7a0174..ac29427  v1.2-deformation -> v1.2-deformation</code>. **That line is your confirmation.** If it is missing, nothing was pushed.

## Line-ending warnings

On Windows Git prints many lines such as <code>warning: LF will be replaced by CRLF the next time Git touches it</code>. They are about how line endings are stored and are harmless here. They are noisy, not errors; the real errors start with <code>error:</code> or <code>fatal:</code>.

## Merging to main

When the work on a branch is finished and tested, it is merged into <code>main</code>, normally through a **pull request** on GitHub (so that the change is reviewed and the history is clear). From the branch: <code>git push</code>, then open the pull request from <code>v1.2-deformation</code> into <code>main</code>. After the merge, a **tag** marks the release (<code>git tag v1.2</code> then <code>git push origin v1.2</code>), so that the exact source of every release can be found later. These two steps were not needed during development of 1.2, which all happened on one branch, but are the normal route to a public release.

# Updating This Guide

The guide you are reading is built from plain text. Each chapter is a Markdown file in <code>docs/src/suite/</code>, in the order of their file names (<code>01-welcome.md</code>, <code>02-design.md</code>, and so on). To change it, edit a file, then build:

```powershell
cd docs\src
python build_suite_guide.py
```

The builder converts the Markdown to a web page, prints it to a PDF with the project's Electron, adds the contents page and the page numbers, and writes <code>docs/Vibration Lab Suite - User Guide.pdf</code>. The packaging script copies that PDF into <code>Documents</code>.

A few conventions of the Markdown:

* <code>#</code> starts a chapter, <code>##</code> a section.
* A line <code>::: note</code> to <code>:::</code> makes a call-out box. The kinds are <code>note</code>, <code>tip</code>, <code>warn</code>, <code>why</code>, <code>try</code>, <code>def</code> and <code>mapx</code> (a maths-to-code table).
* An equation is <code>&lt;div class="eq"&gt;...&lt;span class="tag"&gt;(5.1)&lt;/span&gt;&lt;/div&gt;</code>.
* A **real source listing** is one line: <code>@@src core/src/world.cpp | start-regex | end-regex</code>. The builder finds the first line matching the start pattern and the first line after it matching the end pattern (or <code>+N</code> for N lines) and copies them, with their real line numbers. If a pattern no longer matches, **the build stops with an error**, which is the point: a listing can never describe code that has since been renamed.
* A screenshot is <code>{{img:name.png|caption}}</code>.

After changing the guide, commit the Markdown *and* the rebuilt PDF, and rebuild the package so that <code>dist</code> carries the new PDF.

# Troubleshooting a Release

| Symptom | Cause | Fix |
|---------|-------|-----|
| <code>g++</code> exits with an error and prints nothing | The compiler folder is not on the PATH of this terminal | <code>$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"</code> |
| <code>PermissionError ... VibrationLabSuite.exe</code> | The program is running from <code>dist</code> | Close it, restore <code>dist</code> if it was half rebuilt, package again |
| "validation suite FAILED: not packaging" | A physics check failed | Read the FAIL line: it prints measured and expected values. Fix the code or the test, never the threshold alone |
| A test that passed before now fails after an unrelated edit | The edit changed behaviour, for example a copy-paste of attachment points in the energy function | Use the energy-ledger test: it fails when forces and energy disagree. Compare the force loop with the energy loop |
| The program shows an old value after you changed a default | A saved <code>last_session.vlab</code> or a saved design holds the old value | Change the loader to override it, or re-save the design. Defaults only apply to new designs |
| A new tool crashes the program when chosen | A table indexed by the tool number has no entry for it | Search for arrays indexed by <code>(int)tool</code> and extend them (and bound-check them) |
| The package works but the Unreal plugin differs | <code>ue5/sync_core.py</code> was not run | Run it, commit <code>ue5/</code> |
| <code>git push</code> says nothing was pushed or is rejected | The branch moved on the remote, or you are on the wrong branch | <code>git fetch</code>, <code>git log --oneline origin/branch -3</code>, then merge or rebase before pushing. Never force-push to share a branch |
| The PDF build stops with "start pattern not found" | A listing refers to code that was renamed | Update the <code>@@src</code> pattern to the new name |

# The Release Checklist

Print this list. Every item was needed at least once during 1.2.

1. <code>git status --short</code>: no surprises, no unexpected <code>D</code> lines.
2. <code>cmake --build build</code> succeeds with no errors.
3. <code>.\build\vlab_tests.exe</code>: **all checks pass**, including the energy-ledger check for every example.
4. Run <code>build\vlab_suite.exe</code> and use every feature you changed with the mouse, including selecting, editing, deleting and saving and reopening.
5. <code>python ue5\sync_core.py</code> if anything under <code>core</code> changed.
6. Rebuild the guide if the documentation changed.
7. **Close every running copy of the program.**
8. <code>python tools\package.py</code>: ends with the <code>PACKAGED:</code> lines, and reports all checks passed.
9. Optionally the safety copy: <code>VLAB_DIST</code> set to <code>dist-1.2</code>, package again, remove the variable.
10. <code>git status --short</code> again. The changes under <code>dist</code> should be modifications (<code>M</code>) only. Deletions mean the packaging was interrupted.
11. <code>git add -u</code>, add new files by name, <code>git commit</code> with a clear message.
12. <code>git push origin &lt;branch&gt;</code> and **read the last line of its output**.
13. Confirm both destinations are current: <code>git log -1 --format=%h</code> equals <code>git log origin/&lt;branch&gt; -1 --format=%h</code>, and the executable in <code>dist</code> is newer than your last source change.

::: why
**Why so many steps for a small change?** Each step catches a different class of mistake. The compiler catches typos, the tests catch wrong physics, the window catches wrong interfaces, the sync catches a diverging plugin, the packaging gate catches a broken release, and the final comparison catches the commonest slip of all: changes made and tested but never delivered. Each was added after the mistake it prevents actually happened.
:::
