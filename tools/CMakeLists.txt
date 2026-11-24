add_executable(zerofin_jf_probe jf_probe.cpp)
target_link_libraries(zerofin_jf_probe PRIVATE zerofin_http_curl)
target_compile_options(zerofin_jf_probe PRIVATE -Wall -Wextra)
