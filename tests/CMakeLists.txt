FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(doctest)
list(APPEND CMAKE_MODULE_PATH ${doctest_SOURCE_DIR}/scripts/cmake)
include(doctest)

add_executable(zerofin_tests
  test_main.cpp
  auth_test.cpp
  cards_test.cpp
  circuit_background_test.cpp
  client_test.cpp
  device_profile_test.cpp
  demo_http_test.cpp
  discovery_test.cpp
  format_test.cpp
  grid_cursor_test.cpp
  hdmi_test.cpp
  keys_test.cpp
  list_cursor_test.cpp
  menu_tabs_test.cpp
  models_test.cpp
  output_plan_test.cpp
  output_watch_test.cpp
  mpv_options_test.cpp
  player_backend_test.cpp
  player_controls_test.cpp
  player_timers_test.cpp
  image_cache_test.cpp
  image_fetch_test.cpp
  request_queue_test.cpp
  playlist_test.cpp
  playback_decision_test.cpp
  resume_test.cpp
  status_info_test.cpp
  storage_test.cpp
  text_edit_test.cpp
  url_test.cpp
  volume_test.cpp)
target_link_libraries(zerofin_tests PRIVATE zerofin_player zerofin_jellyfin zerofin_storage zerofin_ui_core doctest::doctest)
target_compile_options(zerofin_tests PRIVATE -Wall -Wextra)

doctest_discover_tests(zerofin_tests)
