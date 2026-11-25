add_library(zerofin_jellyfin STATIC
  auth.cpp
  client.cpp
  curl_cli_http.cpp
  demo_http.cpp
  device_profile.cpp
  discovery.cpp
  image_fetch.cpp
  models.cpp
  playback_decision.cpp
  resume.cpp
  url.cpp)
target_include_directories(zerofin_jellyfin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(zerofin_jellyfin PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(zerofin_jellyfin PRIVATE -Wall -Wextra)

if(ZEROFIN_WITH_CURL)
  find_package(CURL REQUIRED)
  add_library(zerofin_http_curl STATIC curl_http.cpp)
  target_include_directories(zerofin_http_curl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
  target_link_libraries(zerofin_http_curl PUBLIC zerofin_jellyfin CURL::libcurl)
  target_compile_options(zerofin_http_curl PRIVATE -Wall -Wextra)
endif()
