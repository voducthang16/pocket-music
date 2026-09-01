cmake_minimum_required(VERSION 3.20)
project(pocket_music VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(TagLib CONFIG REQUIRED)

add_executable(pocket-music
  src/main.cpp
  src/app/application.cpp
  src/app/navigation.cpp
  src/core/atomic_file.cpp
  src/core/library.cpp
  src/core/player.cpp
  src/core/state.cpp
  src/settings/preferences.cpp
  src/ui/input.cpp
  src/ui/primitives.cpp
  src/ui/renderer.cpp
  src/ui/theme.cpp
)

target_include_directories(pocket-music PRIVATE src)
target_compile_options(pocket-music PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(pocket-music PRIVATE
  SDL2::SDL2
  SDL2_ttf::SDL2_ttf
  SDL2_image::SDL2_image
  TagLib::tag
)

if(APPLE)
  target_link_libraries(pocket-music PRIVATE "-framework CoreFoundation")
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(pocket-music-tests
    tests/test_main.cpp
    tests/test_library.cpp
    tests/test_navigation.cpp
    tests/test_player.cpp
    tests/test_preferences.cpp
    tests/test_state.cpp
    src/core/atomic_file.cpp
    src/core/library.cpp
    src/core/player.cpp
    src/core/state.cpp
    src/settings/preferences.cpp
    src/app/navigation.cpp
    src/ui/primitives.cpp
    src/ui/theme.cpp
  )
  target_include_directories(pocket-music-tests PRIVATE src)
  target_compile_options(pocket-music-tests PRIVATE -Wall -Wextra -Wpedantic)
  target_link_libraries(pocket-music-tests PRIVATE TagLib::tag SDL2::SDL2 SDL2_ttf::SDL2_ttf)

  add_test(NAME unit COMMAND pocket-music-tests)
  add_test(NAME smoke COMMAND pocket-music --smoke-test --music ${CMAKE_SOURCE_DIR}/Music)
endif()
