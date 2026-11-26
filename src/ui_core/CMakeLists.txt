add_library(zerofin_ui_core STATIC
  format.cpp
  keys.cpp
  player_controls.cpp
  player_timers.cpp
  playlist.cpp
  status_info.cpp
  list_cursor.cpp
  menu_tabs.cpp
  grid_cursor.cpp
  request_queue.cpp
  cards.cpp
  circuit_background.cpp
  text_edit.cpp)
target_include_directories(zerofin_ui_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(zerofin_ui_core PUBLIC zerofin_jellyfin)
target_compile_options(zerofin_ui_core PRIVATE -Wall -Wextra)
