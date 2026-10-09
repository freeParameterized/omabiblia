cmake_minimum_required(VERSION 3.16)
project(omabiblia VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client)

# Vendored GLFW 3.4 (Wayland + X11)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/glfw EXCLUDE_FROM_ALL)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/thirdparty/imgui)
add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_widgets.cpp ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_DIR})
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_executable(omabiblia
  src/main.cpp src/gl.cpp src/theme.cpp src/bible.cpp src/render.cpp src/audio.cpp src/screen.cpp
  src/scene_crawl.cpp src/scene_link.cpp src/scene_command.cpp src/scene_hacker.cpp src/scene_retro.cpp
  src/wl_input.cpp)
target_include_directories(omabiblia PRIVATE src thirdparty ${IMGUI_DIR})
# Dev builds also look for data/ next to the source tree; release builds must not embed a local path.
option(OMA_DEV_SOURCE_DIR "Let the binary find data/ in this source tree (dev builds)" ON)
if(OMA_DEV_SOURCE_DIR)
  target_compile_definitions(omabiblia PRIVATE OMA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endif()
target_link_libraries(omabiblia PRIVATE imgui glfw OpenGL::GL PkgConfig::WAYLAND Threads::Threads dl m)
target_compile_options(omabiblia PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
# audio.cpp carries miniaudio's implementation; keep its warnings out of ours
set_source_files_properties(src/audio.cpp src/screen.cpp src/render.cpp PROPERTIES COMPILE_OPTIONS "-w")

install(TARGETS omabiblia RUNTIME DESTINATION bin)
install(DIRECTORY data assets DESTINATION share/omabiblia)
