cmake_minimum_required(VERSION 3.16)
project(furnace-presenter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build furnace engine as a static library
set(FURNACE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/external/furnace)
add_subdirectory(external/furnace furnace-build)

set(IMGUIPATH ${FURNACE_ROOT}/extern/imgui_patched)

set(IMGUISRC
    ${IMGUIPATH}/imgui.cpp
    ${IMGUIPATH}/imgui_draw.cpp
    ${IMGUIPATH}/imgui_widgets.cpp
    ${IMGUIPATH}/imgui_tables.cpp
    ${IMGUIPATH}/backends/imgui_impl_sdl2.cpp
    ${IMGUIPATH}/backends/imgui_impl_opengl2.cpp
)

# Header-only dependencies
set(FURNACE_PRESENTER_DEPS
  ${CMAKE_CURRENT_SOURCE_DIR}/deps
  ${FURNACE_ROOT}/src
  ${FURNACE_ROOT}/extern
  ${FURNACE_ROOT}/extern/blip_buf
  ${FURNACE_ROOT}/extern/fmt/include
  ${IMGUIPATH}
)

set(SOURCES
    src/main.cpp
    src/cli.cpp
    src/config.cpp
    src/renderer.cpp
    src/video_builder.cpp
    src/visualizer.cpp
    src/oscilloscope.cpp
    src/piano_roll.cpp
    src/font.cpp
    src/filters.cpp
    src/gui.cpp
)

add_executable(furnace-presenter ${SOURCES} ${IMGUISRC})

target_include_directories(furnace-presenter PRIVATE ${FURNACE_PRESENTER_DEPS})
target_compile_definitions(furnace-presenter PRIVATE HAVE_GUI=1)
target_link_libraries(furnace-presenter PRIVATE furnace-engine)

if(WIN32)
  target_link_libraries(furnace-presenter PRIVATE shlwapi opengl32)
endif()

# Copy assets to build directory
add_custom_command(TARGET furnace-presenter POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:furnace-presenter>/assets
)
