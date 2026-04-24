cmake_minimum_required(VERSION 3.16)
project(furnace-presenter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# Configure Furnace and synthesize the engine target locally.
set(FURNACE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/external/furnace)
add_subdirectory(external/furnace furnace-build)

function(furnace_presenter_get_dir_var out name)
  get_directory_property(_value DIRECTORY ${FURNACE_ROOT} DEFINITION ${name})
  set(${out} "${_value}" PARENT_SCOPE)
endfunction()

function(furnace_presenter_prefix_relative_paths out base)
  set(_result)
  foreach(_item IN LISTS ARGN)
    if(NOT _item)
      continue()
    endif()
    if(IS_ABSOLUTE "${_item}" OR "${_item}" MATCHES "^\\$<")
      list(APPEND _result "${_item}")
    else()
      list(APPEND _result "${base}/${_item}")
    endif()
  endforeach()
  set(${out} "${_result}" PARENT_SCOPE)
endfunction()

if(NOT TARGET furnace-engine)
  furnace_presenter_get_dir_var(FURNACE_ENGINE_SOURCES ENGINE_SOURCES)
  furnace_presenter_get_dir_var(FURNACE_AUDIO_SOURCES AUDIO_SOURCES)
  furnace_presenter_get_dir_var(FURNACE_CLI_SOURCES CLI_SOURCES)
  furnace_presenter_get_dir_var(FURNACE_DEPENDENCY_INCLUDES DEPENDENCIES_INCLUDE_DIRS)
  furnace_presenter_get_dir_var(FURNACE_DEPENDENCY_COMPILE_OPTIONS DEPENDENCIES_COMPILE_OPTIONS)
  furnace_presenter_get_dir_var(FURNACE_DEPENDENCY_DEFINES DEPENDENCIES_DEFINES)
  furnace_presenter_get_dir_var(FURNACE_DEPENDENCY_LIBRARIES DEPENDENCIES_LIBRARIES)
  furnace_presenter_get_dir_var(FURNACE_DEPENDENCY_LIBRARY_DIRS DEPENDENCIES_LIBRARY_DIRS)
  furnace_presenter_get_dir_var(FURNACE_DEPENDENCY_LINK_OPTIONS DEPENDENCIES_LINK_OPTIONS)
  furnace_presenter_get_dir_var(FURNACE_DEPENDENCY_LEGACY_LDFLAGS DEPENDENCIES_LEGACY_LDFLAGS)

  furnace_presenter_prefix_relative_paths(FURNACE_ENGINE_SOURCES_ABS ${FURNACE_ROOT} ${FURNACE_ENGINE_SOURCES})
  furnace_presenter_prefix_relative_paths(FURNACE_AUDIO_SOURCES_ABS ${FURNACE_ROOT} ${FURNACE_AUDIO_SOURCES})
  furnace_presenter_prefix_relative_paths(FURNACE_CLI_SOURCES_ABS ${FURNACE_ROOT} ${FURNACE_CLI_SOURCES})
  furnace_presenter_prefix_relative_paths(FURNACE_DEPENDENCY_INCLUDES_ABS ${FURNACE_ROOT} ${FURNACE_DEPENDENCY_INCLUDES})

  add_library(furnace-engine STATIC EXCLUDE_FROM_ALL
    ${FURNACE_ENGINE_SOURCES_ABS}
    ${FURNACE_AUDIO_SOURCES_ABS}
    ${FURNACE_CLI_SOURCES_ABS}
  )
  target_include_directories(furnace-engine SYSTEM PUBLIC ${FURNACE_DEPENDENCY_INCLUDES_ABS})
  target_compile_options(furnace-engine PUBLIC ${FURNACE_DEPENDENCY_COMPILE_OPTIONS})
  target_compile_definitions(furnace-engine PUBLIC ${FURNACE_DEPENDENCY_DEFINES})
  target_link_libraries(furnace-engine PUBLIC ${FURNACE_DEPENDENCY_LIBRARIES})
  if(WIN32)
    get_target_property(_fe_libs furnace-engine LINK_LIBRARIES)
    if(_fe_libs)
      list(REMOVE_ITEM _fe_libs SDL2main)
      set_target_properties(furnace-engine PROPERTIES LINK_LIBRARIES "${_fe_libs}")
    endif()
  endif()
  if("${CMAKE_VERSION}" VERSION_LESS "3.13")
    if(FURNACE_DEPENDENCY_LEGACY_LDFLAGS)
      target_link_libraries(furnace-engine PUBLIC ${FURNACE_DEPENDENCY_LEGACY_LDFLAGS})
    endif()
  else()
    if(FURNACE_DEPENDENCY_LIBRARY_DIRS)
      target_link_directories(furnace-engine PUBLIC ${FURNACE_DEPENDENCY_LIBRARY_DIRS})
    endif()
    if(FURNACE_DEPENDENCY_LINK_OPTIONS)
      target_link_options(furnace-engine PUBLIC ${FURNACE_DEPENDENCY_LINK_OPTIONS})
    endif()
  endif()
endif()

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
  ${FURNACE_ROOT}/extern/libsndfile-modified/include
  ${FURNACE_ROOT}/extern/SDL/include
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
target_compile_definitions(furnace-presenter PRIVATE HAVE_GUI=1 HAVE_SNDFILE=1)
target_link_libraries(furnace-presenter PRIVATE furnace-engine)

if(WIN32)
  target_link_libraries(furnace-presenter PRIVATE shlwapi opengl32)
endif()

# Copy assets to build directory
add_custom_command(TARGET furnace-presenter POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:furnace-presenter>/assets
)
