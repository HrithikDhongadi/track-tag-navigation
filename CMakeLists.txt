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
