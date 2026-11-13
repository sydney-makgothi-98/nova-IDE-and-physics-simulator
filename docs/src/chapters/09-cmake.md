# CMake: The Recipe Book

## The problem CMake solves

Building a one-file program is easy: `g++ main.cpp -o hello.exe`. Now imagine a project with 200 files, three libraries, different compilers on Windows, Mac and Linux, debug and release versions, and tests. The command would fill a page and break the moment someone moved a file.

**CMake** solves this. You write one recipe, `CMakeLists.txt`, that says *what* to build. CMake then writes the real build instructions for whatever tool and system you have.

::: def CMake
A **build-system generator**. It does **not** compile anything itself. It reads your `CMakeLists.txt` and produces build files for another tool (Ninja, Make, Visual Studio), which then run the compiler.
:::

{{fig:cmake-flow|CMake reads your recipe and writes build files. The build tool then does the compiling and linking.}}

NOVA uses CMake because **CLion does** and because it is the industry standard for C and C++. Every new NOVA project starts with a `CMakeLists.txt`, and the build button runs exactly two commands: *configure* and *build*.

## Your first CMakeLists.txt

This is the file NOVA's New Project wizard creates (and the sample project in the screenshots uses):

```cmake
cmake_minimum_required(VERSION 3.16)
project(hello_nova LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(hello_nova src/main.cpp src/shapes.cpp)
target_include_directories(hello_nova PRIVATE include)
```

| Command | What it says |
|---|---|
| `cmake_minimum_required(VERSION 3.16)` | "I need CMake 3.16 or newer", and use those rules. |
| `project(hello_nova LANGUAGES CXX)` | Name the project; it uses C++. (Use `C` for C, or both.) |
| `set(CMAKE_CXX_STANDARD 17)` | Use the C++17 language rules. |
| `set(CMAKE_CXX_STANDARD_REQUIRED ON)` | Fail if the compiler cannot do C++17. |
| `add_executable(name files...)` | Make a program called `name` from these files. |
| `target_include_directories(name PRIVATE include)` | Let `name` find headers inside the `include` folder. |

::: why Why cmake_minimum_required first?
CMake's behaviour changes between versions. This line pins the rules, so a project written today behaves the same in five years and fails clearly if the CMake is too old, instead of misbehaving silently. Newer CMake versions even refuse very old values.
:::

::: whatif What if you forget the CXX_STANDARD lines?
The compiler uses its own default, which differs between compiler versions. Your code might compile on your machine and fail on a colleague's because they use a different default. Always state the standard you need.
:::

## The two commands

```text
cmake -S . -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
```

1. **Configure**: `-S .` is the **S**ource folder (where `CMakeLists.txt` lives), `-B cmake-build-debug` is the **B**uild folder (where all the generated files go), `-G Ninja` picks the generator, `-D NAME=value` sets a variable.
2. **Build**: `--build` runs the real build tool inside the build folder.

When you press Run in NOVA, its output panel shows exactly these commands. The second line you see is the compiler at work: `[1/2] Building CXX object ...` then `[2/2] Linking CXX executable hello_nova.exe`. The `[1/2]` counter comes from Ninja.

::: why Why a separate build folder?
This is called an **out-of-source build**. All the mess (object files, caches, the executable) goes in one folder that you can delete at any time, and your source folder stays clean. Add the build folder to `.gitignore`. NOVA names it `cmake-build-debug`, the same as CLion, and its New Project wizard writes that into `.gitignore` for you.
:::

## Build types: Debug and Release

| Type | Flags (roughly) | Use for |
|---|---|---|
| `Debug` | `-g -O0` | Developing and debugging: full information, no optimisation, so stepping follows your code line by line |
| `Release` | `-O2 -DNDEBUG` | Shipping: fast, no debug info |

NOVA always builds `Debug`, because its debugger needs the `-g` information. If you built Release and tried to debug, the debugger would jump around unpredictably, because the optimiser rearranges and removes your lines.

## Libraries and linking

Real projects split into **libraries** (reusable code) and **executables** (programs). A target can **link** to a library.

```cmake
cmake_minimum_required(VERSION 3.16)
project(shapes_demo LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. A library: code that other targets can use
add_library(geometry STATIC src/circle.cpp src/rect.cpp)
target_include_directories(geometry PUBLIC include)

# 2. A program that uses the library
add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE geometry)

# 3. A second program: tests
add_executable(tests tests/test_main.cpp)
target_link_libraries(tests PRIVATE geometry)
```

{{fig:target-graph|Targets depend on each other. CMake works out the order to build them.}}

Key ideas:

* `add_library(name STATIC files...)` makes a library. `STATIC` means it is copied into each program that uses it (a `.a` or `.lib` file). `SHARED` makes a DLL/`.so` loaded at run time.
* `target_link_libraries(app PRIVATE geometry)` says "app uses geometry". CMake builds `geometry` first and links it.
* `PUBLIC`, `PRIVATE`, `INTERFACE` describe who needs a setting:

| Keyword | Meaning |
|---|---|
| `PRIVATE` | Only this target needs it. |
| `PUBLIC` | This target **and** anything that links to it needs it. |
| `INTERFACE` | Only the users need it, not the target itself. |

::: why Why does include have PUBLIC on the library?
Anyone who uses `geometry` must also be able to `#include "circle.h"`. `PUBLIC` makes the `include` folder travel along: `app` and `tests` get it **automatically** when they link to `geometry`. If it were `PRIVATE`, `app` would fail with *"circle.h: No such file"* even though the library built perfectly. It is the most common CMake beginner error.
:::

## Variables, conditions and loops

```cmake
set(SOURCES src/a.cpp src/b.cpp src/c.cpp)       # a list
add_executable(app ${SOURCES})                    # ${...} reads a variable

option(ENABLE_WARNINGS "Turn on strict warnings" ON)
if(ENABLE_WARNINGS)
  target_compile_options(app PRIVATE -Wall -Wextra)
endif()

foreach(name IN ITEMS alpha beta)
  message(STATUS "target: ${name}")
endforeach()

if(WIN32)
  target_link_libraries(app PRIVATE ws2_32)
elseif(APPLE)
  message(STATUS "building on a Mac")
endif()
```

`message(STATUS ...)` prints during configure. `message(FATAL_ERROR ...)` stops with an error. `option()` creates a switch that users flip with `-DENABLE_WARNINGS=OFF`.

## Splitting a big project: add_subdirectory

```text
project/
  CMakeLists.txt          <- top level: add_subdirectory(lib) and add_subdirectory(app)
  lib/
    CMakeLists.txt        <- add_library(geometry ...)
    include/geometry/circle.h
    src/circle.cpp
  app/
    CMakeLists.txt        <- add_executable(app ...) and target_link_libraries(app PRIVATE geometry)
    main.cpp
```

Each folder owns its own small recipe. The top file simply says `add_subdirectory(lib)` then `add_subdirectory(app)`.

::: why Why many small files?
So that each part can be understood, changed and reused alone, exactly like source code. NOVA's scanner finds *every* `CMakeLists.txt` in your project and reads each `add_executable(...)` to fill the run-configuration drop-down.
:::

## Testing with CTest

```cmake
enable_testing()
add_test(NAME area_test COMMAND tests)
```

After building, run `ctest` in the build folder. A test passes if the program returns **exit code 0**, the same code you met in Chapters 9 and 10. That is why checking exit codes matters.

## Fetching other people's code

```cmake
include(FetchContent)
FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt GIT_TAG 10.2.1)
FetchContent_MakeAvailable(fmt)
target_link_libraries(app PRIVATE fmt::fmt)
```

CMake downloads the library and builds it with your project. `find_package(SomeLib REQUIRED)` instead looks for a library that is already installed.

## compile_commands.json: how clangd learns your flags

Add this line to your configure command (NOVA does):

```text
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

CMake then writes `compile_commands.json` in the build folder: a list that says, for every source file, the **exact compiler command** used.

```json
[
  {
    "directory": "C:/proj/cmake-build-debug",
    "command": "g++ -IC:/proj/include -g -std=gnu++17 -o CMakeFiles/app.dir/src/main.cpp.obj -c C:/proj/src/main.cpp",
    "file": "C:/proj/src/main.cpp"
  }
]
```

::: why Why does the language server need it?
To understand `#include "shapes.h"` clangd must know where the `include` folder is. To understand `std::optional` it must know you are using C++17. The compile database hands it every flag, so its idea of your code matches the compiler's. Without it clangd would guess, and show false errors on perfectly good code. NOVA runs a silent configure when you open a CMake project, just to get this file.
:::

## A real NOVA story

::: story The fallback database
For folders that have **no** CMakeLists.txt (a few loose `.cpp` files) NOVA creates its own tiny `compile_commands.json` in a hidden `.nova` folder, listing each file with the C++ standard from the settings. Then clangd works there too. It is a nice example of generating a needed file when the "official" source is missing.
:::

## Common errors and fixes

| Message | Likely cause | Fix |
|---|---|---|
| `Could not find CMAKE_CXX_COMPILER` | No compiler on PATH | Install MinGW/MSYS2 or LLVM and reopen NOVA |
| `No CMAKE_CXX_COMPILER could be found` after changing compilers | Stale cache | Delete the build folder and configure again |
| `undefined reference to 'foo'` | Source file not listed in `add_executable` | Add the `.cpp` to the target |
| `fatal error: x.h: No such file` | Missing include directory | `target_include_directories(... PRIVATE include)` |
| `CMake Error ... CMakeLists.txt:N` | Syntax mistake on that line | Check brackets and spelling at line N |
| Nothing rebuilds | Wrong build folder | Check the `-B` path |

::: warn Watch out
When you add a new `.cpp` file you must add it to `CMakeLists.txt` (or use a glob) and **reconfigure**. NOVA marks the project "dirty" whenever you save a `CMakeLists.txt`, so the next Run reconfigures automatically.
:::

## Mini assignments

::: assign Assignment 12.1: Hello CMake
Make a folder with `main.cpp` (print a message) and a `CMakeLists.txt`. Configure and build it from a terminal using the two commands, then run the program. Find the `.exe` in the build folder.
:::

::: assign Assignment 12.2: Two files
Move a function into `util.cpp` with `util.h`. Make it build. Then deliberately forget to list `util.cpp` and read the linker error. Fix it.
:::

::: assign Assignment 12.3: A switch
Add `option(VERBOSE "Print extra output" OFF)` and use `target_compile_definitions` so that `#ifdef VERBOSE` code in your program compiles only when you pass `-DVERBOSE=ON`.
:::

::: assign Assignment 12.4: Read compile_commands.json
Configure a project with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`. Open the JSON file and find the exact command for one source file. Identify the include path and the language standard.
:::

## Big assignment: A multi-target project

::: big "GeoKit": library, app, tests, options
**Goal.** Build a small but realistic project with the structure of a professional C++ code base.

**Requirements.**

1. Folder layout: `lib/` (library), `app/` (program), `tests/` (tests), top-level `CMakeLists.txt`, `.gitignore`.
2. Library `geometry` (STATIC) with classes `Circle`, `Rect` and a free function `totalArea(const std::vector<std::unique_ptr<Shape>>&)`. Use the polymorphism example from Chapter 11. Public headers in `lib/include/geometry/`.
3. Program `geokit` that reads shapes from a text file (one per line, for example `circle 2.5`) and prints each area and the total.
4. A `tests` executable that checks at least six cases with a small `check(condition, message)` helper and returns a non-zero exit code if any check fails. Register it with `add_test`.
5. Options: `GEOKIT_WARNINGS` (default ON, adds `-Wall -Wextra -Wpedantic`) and `GEOKIT_SANITIZE` (default OFF, adds `-fsanitize=address,undefined` for compile **and** link).
6. Use `target_include_directories` with correct `PUBLIC`/`PRIVATE` choices, and prove they are right by removing `PUBLIC` and reading the error.
7. Export the compile commands and confirm clangd (in NOVA) shows no false errors in any file.
8. Open the folder in NOVA: the run-configuration drop-down must list `geokit` and `tests`. Run both, and debug `geokit` with a breakpoint inside `Circle::area`.

**Stretch goals.** Add an `install` rule. Add a `Release` build and compare run times. Pull in a testing framework (Catch2 or GoogleTest) using `FetchContent`.

**How to check yourself.** Delete the build folder, press Run in NOVA, and the whole project must configure, build and run with no manual steps.
:::

## Quiz

::: quiz
1. Does CMake compile code? Explain.
2. What do `-S` and `-B` mean?
3. Why use a separate build folder?
4. What is the difference between `Debug` and `Release`?
5. What do `PUBLIC` and `PRIVATE` mean in `target_include_directories`?
6. What does `option()` create?
7. Why does the language server need `compile_commands.json`?
8. What error appears if a `.cpp` is missing from `add_executable`?
9. Why does `add_test` depend on exit codes?
10. Why does NOVA build in Debug mode?
:::

::: answers
1. No. It writes build files for another tool (Ninja, Make...), which runs the compiler.
2. `-S` is the source folder; `-B` is the build folder.
3. It keeps generated files out of your sources and can be deleted safely.
4. Debug has debugging information and no optimisation; Release is optimised and has no debug info.
5. `PRIVATE` is for the target only; `PUBLIC` is for the target and everything that links to it.
6. A yes/no switch that users can change with `-DNAME=ON/OFF`.
7. It lists the exact flags (include folders, language standard) for every file, so clangd understands the code the same way the compiler does.
8. A linker error: `undefined reference to ...`.
9. A test passes when its program returns exit code 0 and fails otherwise.
10. The debugger needs the `-g` information, and without optimisation, stepping follows the source lines.
:::
