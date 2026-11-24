cmake_minimum_required(VERSION 3.25)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" ZEROFIN_VERSION)
string(STRIP "${ZEROFIN_VERSION}" ZEROFIN_VERSION)

project(ZeroFin VERSION ${ZEROFIN_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(ZEROFIN_BUILD_TESTS "Build the unit tests" ON)
option(ZEROFIN_BUILD_APP "Build the LVGL application" ON)
option(ZEROFIN_WITH_CURL "Build the libcurl HTTP transport" ON)
# USE_DESKTOP=ON builds the SDL simulator. OFF builds for the device (Linux fbdev + evdev).
if(APPLE)
  set(_zerofin_desktop_default ON)
else()
  set(_zerofin_desktop_default OFF)
endif()
option(USE_DESKTOP "Build the SDL desktop simulator" ${_zerofin_desktop_default})

include(FetchContent)

# Some pinned dependencies declare an old CMake minimum. CMake 4 rejects that by default.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# JSON parser, pinned.
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d)
FetchContent_MakeAvailable(nlohmann_json)

# libmpv plays the video. The option is ON when pkg-config finds it. Without libmpv the player
# only has the demo backend (ZEROFIN_DEMO=1).
find_package(PkgConfig QUIET)
set(_zerofin_mpv_found OFF)
if(PkgConfig_FOUND)
  pkg_check_modules(MPV QUIET IMPORTED_TARGET mpv)
  if(MPV_FOUND)
    set(_zerofin_mpv_found ON)
  endif()
endif()
option(ZEROFIN_WITH_MPV "Use libmpv for playback" ${_zerofin_mpv_found})

add_subdirectory(src/jellyfin)
add_subdirectory(src/player)
add_subdirectory(src/storage)
add_subdirectory(src/ui_core)

if(ZEROFIN_BUILD_APP)
  # LVGL 9.5, pinned. One lv_conf.h for the simulator and the device.
  set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
  set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/config/lv_conf.h" CACHE PATH "" FORCE)
  FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.5.0
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(lvgl)

  find_package(Freetype REQUIRED)
  target_link_libraries(lvgl PUBLIC Freetype::Freetype)
  if(USE_DESKTOP)
    find_package(SDL2 CONFIG REQUIRED)
    target_link_libraries(lvgl PUBLIC SDL2::SDL2)
    target_compile_definitions(lvgl PUBLIC USE_DESKTOP=1)
  else()
    target_compile_definitions(lvgl PUBLIC USE_DESKTOP=0)
  endif()

  add_subdirectory(src/app)

  # The .deb for the device. Not for the simulator.
  if(NOT USE_DESKTOP)
    include(cmake/package.cmake)
  endif()
endif()

if(ZEROFIN_WITH_CURL)
  add_subdirectory(tools)
endif()

if(ZEROFIN_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
