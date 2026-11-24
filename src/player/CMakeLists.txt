add_library(zerofin_player STATIC
  demo_backend.cpp
  factory.cpp)
target_include_directories(zerofin_player PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(zerofin_player PUBLIC zerofin_jellyfin)
target_compile_options(zerofin_player PRIVATE -Wall -Wextra)

if(ZEROFIN_WITH_MPV)
  if(NOT TARGET PkgConfig::MPV)
    message(FATAL_ERROR "ZEROFIN_WITH_MPV is ON but pkg-config did not find libmpv (install libmpv-dev)")
  endif()
  target_sources(zerofin_player PRIVATE mpv_backend.cpp)
  target_link_libraries(zerofin_player PRIVATE PkgConfig::MPV)
  target_compile_definitions(zerofin_player PUBLIC ZEROFIN_HAS_MPV=1)
endif()
