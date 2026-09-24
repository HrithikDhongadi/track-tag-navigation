cmake_minimum_required(VERSION 3.16)
project(track_tag_navigation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# `build/track-tag-navigation` is the supported single entry point. It starts Fortress and
# the single native controller process from this same Release build.
configure_file(scripts/track_tag_navigation.in ${CMAKE_BINARY_DIR}/track-tag-navigation @ONLY)
file(CHMOD ${CMAKE_BINARY_DIR}/track-tag-navigation
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

find_package(ignition-transport11 REQUIRED)
find_package(ignition-msgs8 REQUIRED)
find_package(Threads REQUIRED)

find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZBAR REQUIRED IMPORTED_TARGET zbar)

add_executable(track_tag_navigation_controller
  src/main.cpp
  src/options.cpp
  src/control_config.cpp
  src/navigation.cpp
  src/route_manager.cpp
  src/mission_executor.cpp
  src/runtime.cpp
  src/line_follower.cpp
  src/qr_reader.cpp)
target_include_directories(track_tag_navigation_controller PRIVATE include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(track_tag_navigation_controller PRIVATE
  ignition-transport11::core
  ignition-msgs8::ignition-msgs8
  ${OpenCV_LIBS}
  PkgConfig::ZBAR
  Threads::Threads)
target_compile_options(track_tag_navigation_controller PRIVATE -Wall -Wextra -Wpedantic)

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui" CACHE PATH
    "Path to a Dear ImGui source checkout")
if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp" AND EXISTS "$ENV{HOME}/Downloads/imgui/imgui.cpp")
  set(IMGUI_DIR "$ENV{HOME}/Downloads/imgui")
endif()
if(EXISTS "${IMGUI_DIR}/imgui.cpp")
  find_package(OpenGL REQUIRED)
  pkg_check_modules(GLFW REQUIRED IMPORTED_TARGET glfw3)
  add_executable(track_tag_navigation_ui
    src/ui/telemetry_viewer.cpp
    src/ui/dashboard.cpp
    src/ui/telemetry_store.cpp
    src/ui/theme.cpp
    src/ui/widgets.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
  target_include_directories(track_tag_navigation_ui PRIVATE include ${IMGUI_DIR} ${IMGUI_DIR}/backends)
  target_link_libraries(track_tag_navigation_ui PRIVATE
    ignition-transport11::core
    ignition-msgs8::ignition-msgs8
    PkgConfig::GLFW
    OpenGL::GL
    Threads::Threads)
  target_compile_options(track_tag_navigation_ui PRIVATE -Wall -Wextra -Wpedantic)
else()
  message(STATUS "track_tag_navigation_ui disabled: set IMGUI_DIR to a Dear ImGui checkout")
endif()
