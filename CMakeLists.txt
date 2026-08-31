cmake_minimum_required(VERSION 3.20)
project(classic_ipod VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(TagLib CONFIG REQUIRED)

add_executable(classic-ipod
  src/main.cpp
  src/app/application.cpp
  src/app/navigation.cpp
  src/core/library.cpp
  src/core/player.cpp
  src/core/state.cpp
  src/ui/input.cpp
  src/ui/renderer.cpp
)

target_include_directories(classic-ipod PRIVATE src)
target_compile_options(classic-ipod PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(classic-ipod PRIVATE
  SDL2::SDL2
  SDL2_ttf::SDL2_ttf
  SDL2_image::SDL2_image
  TagLib::tag
)

if(APPLE)
  target_link_libraries(classic-ipod PRIVATE "-framework CoreFoundation")
endif()

include(CTest)
if(BUILD_TESTING)
  add_test(NAME smoke COMMAND classic-ipod --smoke-test)
endif()
