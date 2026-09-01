cmake_minimum_required(VERSION 3.20)
project(pocket_music VERSION 0.1.0 LANGUAGES CXX)

option(POCKET_MUSIC_TRIMUI "Build for TrimUI Stock OS" OFF)

if(POCKET_MUSIC_TRIMUI)
  set(CMAKE_CXX_STANDARD 17)
else()
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG QUIET)
if(NOT TARGET SDL2::SDL2)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
endif()

find_package(SDL2_ttf CONFIG QUIET)
if(NOT TARGET SDL2_ttf::SDL2_ttf)
  find_path(SDL2_TTF_INCLUDE_DIR SDL_ttf.h PATH_SUFFIXES SDL2 REQUIRED)
  find_library(SDL2_TTF_LIBRARY NAMES SDL2_ttf REQUIRED)
  add_library(SDL2_ttf::SDL2_ttf UNKNOWN IMPORTED)
  set_target_properties(SDL2_ttf::SDL2_ttf PROPERTIES
    IMPORTED_LOCATION "${SDL2_TTF_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_TTF_INCLUDE_DIR}"
  )
endif()

find_package(SDL2_image CONFIG QUIET)
if(NOT TARGET SDL2_image::SDL2_image)
  find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h PATH_SUFFIXES SDL2 REQUIRED)
  find_library(SDL2_IMAGE_LIBRARY NAMES SDL2_image REQUIRED)
  add_library(SDL2_image::SDL2_image UNKNOWN IMPORTED)
  set_target_properties(SDL2_image::SDL2_image PROPERTIES
    IMPORTED_LOCATION "${SDL2_IMAGE_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_IMAGE_INCLUDE_DIR}"
  )
endif()

find_package(TagLib CONFIG REQUIRED)
find_package(Threads REQUIRED)

find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h
  HINTS /opt/homebrew/opt/ffmpeg/include
  REQUIRED
)
find_library(AVFORMAT_LIBRARY avformat HINTS /opt/homebrew/opt/ffmpeg/lib REQUIRED)
find_library(AVCODEC_LIBRARY avcodec HINTS /opt/homebrew/opt/ffmpeg/lib REQUIRED)
find_library(AVUTIL_LIBRARY avutil HINTS /opt/homebrew/opt/ffmpeg/lib REQUIRED)
find_library(SWRESAMPLE_LIBRARY swresample HINTS /opt/homebrew/opt/ffmpeg/lib REQUIRED)
add_library(FFmpeg::audio INTERFACE IMPORTED)
set_target_properties(FFmpeg::audio PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}"
  INTERFACE_LINK_LIBRARIES
    "${AVFORMAT_LIBRARY};${AVCODEC_LIBRARY};${SWRESAMPLE_LIBRARY};${AVUTIL_LIBRARY}"
)

add_executable(pocket-music
  src/main.cpp
  src/app/application.cpp
  src/app/navigation.cpp
  src/core/atomic_file.cpp
  src/core/ffmpeg_audio_decoder.cpp
  src/core/ffmpeg_sdl_player.cpp
  src/core/library.cpp
  src/core/playback_controller.cpp
  src/core/playback_queue.cpp
  src/core/playback_session.cpp
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
  FFmpeg::audio
  TagLib::tag
  Threads::Threads
)

if(POCKET_MUSIC_TRIMUI)
  target_sources(pocket-music PRIVATE platform/trimui/fcntl_compat.cpp)
  target_link_libraries(pocket-music PRIVATE stdc++fs)
endif()

if(APPLE)
  target_link_libraries(pocket-music PRIVATE "-framework CoreFoundation")
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(pocket-music-tests
    tests/test_main.cpp
    tests/test_library.cpp
    tests/test_navigation.cpp
    tests/test_preferences.cpp
    tests/test_playback.cpp
    tests/test_state.cpp
    src/core/atomic_file.cpp
    src/core/library.cpp
    src/core/playback_controller.cpp
    src/core/playback_queue.cpp
    src/core/playback_session.cpp
    src/core/state.cpp
    src/settings/preferences.cpp
    src/app/navigation.cpp
    src/ui/primitives.cpp
    src/ui/input.cpp
    src/ui/theme.cpp
  )
  target_include_directories(pocket-music-tests PRIVATE src)
  target_compile_options(pocket-music-tests PRIVATE -Wall -Wextra -Wpedantic)
  target_link_libraries(pocket-music-tests PRIVATE
    TagLib::tag
    SDL2::SDL2
    SDL2_ttf::SDL2_ttf
    Threads::Threads
  )

  add_test(NAME unit COMMAND pocket-music-tests)
  add_test(NAME smoke COMMAND pocket-music --smoke-test --music ${CMAKE_SOURCE_DIR}/Music)

  add_executable(pocket-music-ffmpeg-integration
    tests/test_ffmpeg_integration.cpp
    src/core/ffmpeg_audio_decoder.cpp
    src/core/ffmpeg_sdl_player.cpp
  )
  target_include_directories(pocket-music-ffmpeg-integration PRIVATE src)
  target_compile_options(pocket-music-ffmpeg-integration PRIVATE -Wall -Wextra -Wpedantic)
  target_link_libraries(pocket-music-ffmpeg-integration PRIVATE
    FFmpeg::audio
    SDL2::SDL2
    Threads::Threads
  )
  add_test(NAME ffmpeg-integration COMMAND pocket-music-ffmpeg-integration)
endif()
