cmake_minimum_required(VERSION 3.16)
project(VLabSuite CXX)
if(WIN32)
  enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2")
endif()

# ---------------------------------------------------------------- the engine (no OS, no UI)
add_library(vlab_core STATIC
  core/src/model.cpp
  core/src/world.cpp
  core/src/telemetry.cpp
  core/src/analysis.cpp
  core/src/project_io.cpp
  core/src/session.cpp)
target_include_directories(vlab_core PUBLIC core/include)

# ---------------------------------------------------------------- Excel / CSV export (needs zlib)
add_library(vlab_io STATIC io/xlsx.cpp io/report.cpp)
target_include_directories(vlab_io PUBLIC io)
target_link_libraries(vlab_io PUBLIC vlab_core z)

# ---------------------------------------------------------------- command-line runner (batch mode, no window)
add_executable(vlab_cli tools/cli.cpp)
target_link_libraries(vlab_cli PRIVATE vlab_io)
if(MINGW)
  target_link_libraries(vlab_cli PRIVATE -static)
endif()

# ---------------------------------------------------------------- validation suite (console)
add_executable(vlab_tests tests/core_tests.cpp)
target_link_libraries(vlab_tests PRIVATE vlab_core)
if(MINGW)
  target_link_libraries(vlab_tests PRIVATE -static)
endif()

# ---------------------------------------------------------------- the desktop application
if(WIN32 AND EXISTS ${CMAKE_SOURCE_DIR}/app/main.cpp)
  include(app/app.cmake)
endif()
