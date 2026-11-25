add_library(zerofin_storage STATIC config_store.cpp image_cache.cpp)
target_include_directories(zerofin_storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(zerofin_storage PUBLIC zerofin_jellyfin nlohmann_json::nlohmann_json)
target_compile_options(zerofin_storage PRIVATE -Wall -Wextra)
