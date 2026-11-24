#include <doctest/doctest.h>

#include "jellyfin/url.h"

using namespace zerofin::jf;

TEST_CASE("percent_encode keeps unreserved characters and encodes the rest") {
  CHECK(percent_encode("abc-_.~123") == "abc-_.~123");
  CHECK(percent_encode("a b&c/\xC3\xA9") == "a%20b%26c%2F%C3%A9");
  CHECK(percent_encode("") == "");
}

TEST_CASE("join_url uses exactly one slash") {
  CHECK(join_url("http://h:8096/", "/System/Info") == "http://h:8096/System/Info");
  CHECK(join_url("http://h", "a/b") == "http://h/a/b");
  CHECK(join_url("http://h//", "//a") == "http://h/a");
}

TEST_CASE("with_query appends and encodes") {
  CHECK(with_query("http://h/p", {{"a", "1 2"}, {"b", "x"}}) == "http://h/p?a=1%202&b=x");
  CHECK(with_query("http://h/p?x=1", {{"a", "b"}}) == "http://h/p?x=1&a=b");
  CHECK(with_query("http://h/p", {}) == "http://h/p");
}

TEST_CASE("set_query_param replaces a key without regard to case") {
  CHECK(set_query_param("http://h/p?MaxWidth=1920&x=1", "maxWidth", "320") == "http://h/p?maxWidth=320&x=1");
  CHECK(set_query_param("http://h/p", "a", "1") == "http://h/p?a=1");
  CHECK(set_query_param("http://h/p?x=1", "a", "1") == "http://h/p?x=1&a=1");
  CHECK(set_query_param("http://h/p?a=1&A=2&b=3", "a", "9") == "http://h/p?a=9&b=3");
  CHECK(set_query_param("http://h/p?a=1#frag", "a", "2") == "http://h/p?a=2#frag");
}

TEST_CASE("set_query_param writes a negative number as is") {
  CHECK(set_query_param("http://h/p?SubtitleStreamIndex=2", "SubtitleStreamIndex", "-1") ==
        "http://h/p?SubtitleStreamIndex=-1");
}

TEST_CASE("get_query_param decodes and ignores key case") {
  CHECK(get_query_param("http://h/p?searchTerm=spider%20man&x=1", "SearchTerm") == "spider man");
  CHECK(get_query_param("http://h/p?x=1", "y") == std::nullopt);
  CHECK(get_query_param("http://h/p?flag", "flag") == "");
}

TEST_CASE("redact_url hides secrets") {
  CHECK(redact_url("http://h/p?api_key=SECRET&x=1") == "http://h/p?api_key=***&x=1");
  CHECK(redact_url("http://h/p?x=1&ApiKey=SECRET") == "http://h/p?x=1&ApiKey=***");
  CHECK(redact_url("http://h/p?x=1") == "http://h/p?x=1");
}

TEST_CASE("normalize_server_url cleans user input") {
  CHECK(normalize_server_url("jellypi.local:8096") == "http://jellypi.local:8096");
  CHECK(normalize_server_url("  HTTPS://Host/ ") == "https://Host");
  CHECK(normalize_server_url("http://h/jf/") == "http://h/jf");
  CHECK(normalize_server_url("192.168.1.157:8096/") == "http://192.168.1.157:8096");
}

TEST_CASE("normalize_server_url rejects unusable input") {
  CHECK(normalize_server_url("") == std::nullopt);
  CHECK(normalize_server_url("   ") == std::nullopt);
  CHECK(normalize_server_url("http://") == std::nullopt);
  CHECK(normalize_server_url("http://:8096") == std::nullopt);
  CHECK(normalize_server_url("ftp://host") == std::nullopt);
  CHECK(normalize_server_url("a b") == std::nullopt);
}

TEST_CASE("redact_text hides secrets inside free text") {
  using zerofin::jf::redact_text;
  CHECK(redact_text("Opening http://h/v?api_key=SECRET123&x=1 now") == "Opening http://h/v?api_key=***&x=1 now");
  CHECK(redact_text("ApiKey=abc def") == "ApiKey=*** def");
  CHECK(redact_text("GET /v?token=zzz") == "GET /v?token=***");
  CHECK(redact_text("quoted \"http://h/?api_key=abc\" end") == "quoted \"http://h/?api_key=***\" end");
  CHECK(redact_text("nothing secret here") == "nothing secret here");
  CHECK(redact_text("") == "");
}
