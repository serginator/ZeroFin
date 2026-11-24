find_package(Threads REQUIRED)

set(ZEROFIN_APP_SOURCES
  app.cpp
  script_runner.cpp
  screens/detail_misc.cpp
  screens/home_lists.cpp
  screens/open_item.cpp
  screens/player_screen.cpp
  screens/setup_connect.cpp
  ui/async.cpp
  ui/context.cpp
  ui/fonts.cpp
  ui/player_overlay.cpp
  ui/screen.cpp
  ui/widgets.cpp)
list(APPEND ZEROFIN_APP_SOURCES platform/status_reader.cpp)
if(USE_DESKTOP)
  list(APPEND ZEROFIN_APP_SOURCES platform/platform_sdl.cpp)
else()
  list(APPEND ZEROFIN_APP_SOURCES platform/platform_fb.cpp)
endif()

add_library(zerofin_app STATIC ${ZEROFIN_APP_SOURCES})
target_include_directories(zerofin_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zerofin_app PUBLIC lvgl zerofin_jellyfin zerofin_player zerofin_storage zerofin_ui_core Threads::Threads)
target_compile_definitions(zerofin_app PUBLIC
  ZEROFIN_ASSETS_SOURCE_DIR="${PROJECT_SOURCE_DIR}/assets"
  ZEROFIN_VERSION_STRING="${PROJECT_VERSION}")
target_compile_options(zerofin_app PRIVATE -Wall -Wextra)

# On macOS the simulator talks to the LAN through /usr/bin/curl (see make_transport in app.cpp).
if(APPLE)
  set(_zerofin_prefer_cli 1)
else()
  set(_zerofin_prefer_cli 0)
endif()
target_compile_definitions(zerofin_app PRIVATE ZEROFIN_PREFER_CLI_CURL=${_zerofin_prefer_cli})
if(ZEROFIN_WITH_CURL)
  target_link_libraries(zerofin_app PUBLIC zerofin_http_curl)
  target_compile_definitions(zerofin_app PRIVATE ZEROFIN_HAS_LIBCURL=1)
else()
  target_compile_definitions(zerofin_app PRIVATE ZEROFIN_HAS_LIBCURL=0)
endif()

add_executable(zerofin main.cpp)
target_link_libraries(zerofin PRIVATE zerofin_app)
