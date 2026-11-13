# The desktop application: Dear ImGui + ImPlot on Direct3D 11, one self-contained .exe.
set(IMGUI ${CMAKE_SOURCE_DIR}/third_party/imgui)
set(IMPLOT ${CMAKE_SOURCE_DIR}/third_party/implot)

add_executable(vlab_suite WIN32
  app/main.cpp
  app/app_core.cpp
  app/ui_canvas.cpp
  app/ui_panels.cpp
  app/ui_plots.cpp
  app/dialogs.cpp
  app/theme.cpp
  app/section_ui.cpp
  app/dashboard.cpp
  app/video.cpp
  ${IMGUI}/imgui.cpp
  ${IMGUI}/imgui_draw.cpp
  ${IMGUI}/imgui_tables.cpp
  ${IMGUI}/imgui_widgets.cpp
  ${IMGUI}/backends/imgui_impl_win32.cpp
  ${IMGUI}/backends/imgui_impl_dx11.cpp
  ${IMPLOT}/implot.cpp
  ${IMPLOT}/implot_items.cpp
  resources/app.rc)

target_include_directories(vlab_suite PRIVATE app ${IMGUI} ${IMGUI}/backends ${IMPLOT})
target_compile_definitions(vlab_suite PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
target_link_libraries(vlab_suite PRIVATE vlab_io
  d3d11 dxgi d3dcompiler dwmapi gdi32 user32 comdlg32 shell32 ole32 uuid
  mfplat mfreadwrite mfuuid)
if(MINGW)
  # one self-contained exe: no libstdc++/libgcc DLLs to ship
  target_link_options(vlab_suite PRIVATE -static -static-libgcc -static-libstdc++ -mwindows)
endif()
