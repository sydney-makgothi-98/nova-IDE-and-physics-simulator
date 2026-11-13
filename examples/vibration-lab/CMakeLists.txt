cmake_minimum_required(VERSION 3.16)
project(VibrationLab CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Vibration Lab: design a spring / mass / damper system, run it, record it to .mp4.
add_executable(vibration_live live.cpp)

# gdiplus + gdi32 + user32 : window and drawing
# mfreadwrite + mfplat + mfuuid + ole32 : Windows' built-in H.264 video encoder (Media Foundation)
target_link_libraries(vibration_live PRIVATE gdiplus gdi32 user32 mfreadwrite mfplat mfuuid ole32 -static)

# So the program can find design.vlab and write recordings/ next to the source,
# no matter which folder the IDE runs it from.
target_compile_definitions(vibration_live PRIVATE LAB_DIR="${CMAKE_SOURCE_DIR}")
