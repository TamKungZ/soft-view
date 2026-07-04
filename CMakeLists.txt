cmake_minimum_required(VERSION 3.16)
project(soft-view CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(MPV REQUIRED mpv)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

add_executable(soft-view
  src/main.cpp
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(soft-view PRIVATE
  ${SDL2_INCLUDE_DIRS}
  ${MPV_INCLUDE_DIRS}
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)

target_compile_options(soft-view PRIVATE ${SDL2_CFLAGS_OTHER} ${MPV_CFLAGS_OTHER})

target_link_libraries(soft-view PRIVATE
  ${SDL2_LIBRARIES}
  ${MPV_LIBRARIES}
  GL
  dl
  pthread
)

link_directories(${SDL2_LIBRARY_DIRS} ${MPV_LIBRARY_DIRS})
