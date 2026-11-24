#include <doctest/doctest.h>

#include "fixtures.h"
#include "jellyfin/models.h"

using namespace zerofin::jf;

TEST_CASE("parse_item_list reads a movie with media streams") {
  auto list = parse_item_list(fixtures::items_json());
  REQUIRE(list.ok());
  CHECK(list.value().items.size() == 2);
  CHECK(list.value().total == 42);

  const Item& movie = list.value().items[0];
  CHECK(movie.id == "m1");
  CHECK(movie.name == "Movie One");
  CHECK(movie.type == ItemType::Movie);
  CHECK(movie.overview.empty());  // JSON null becomes an empty string
  CHECK(movie.production_year == 1999);
  CHECK(movie.primary_image_tag == "tagA");
  CHECK(movie.user_data.position_ticks == 6'000'000'000);
  CHECK(movie.user_data.play_count == 2);
  CHECK_FALSE(movie.user_data.played);

  REQUIRE(movie.media_sources.size() == 1);
  const MediaSource& src = movie.media_sources[0];
  CHECK(src.id == "ms1");
  CHECK(src.bitrate == 5'200'000);
  REQUIRE(src.video() != nullptr);
  CHECK(src.video()->codec == "h264");
  CHECK(src.video()->width == 1920);
  CHECK(src.video()->bit_depth == 8);
  CHECK(src.video()->fps == doctest::Approx(23.976));
  CHECK(src.audio_streams().size() == 1);
  CHECK(src.subtitle_streams().size() == 2);
  REQUIRE(src.default_audio() != nullptr);
  CHECK(src.default_audio()->index == 1);
  CHECK(src.stream_by_index(3)->codec == "PGSSUB");
  CHECK(src.stream_by_index(99) == nullptr);
}

TEST_CASE("parse_item_list reads an episode") {
  auto list = parse_item_list(fixtures::items_json());
  REQUIRE(list.ok());
  const Item& ep = list.value().items[1];
  CHECK(ep.type == ItemType::Episode);
  CHECK(ep.series_id == "s1");
  CHECK(ep.series_name == "Show");
  CHECK(ep.season_id == "se1");
  CHECK(ep.index_number == 1);
  CHECK(ep.parent_index_number == 1);
  CHECK(ep.media_sources.empty());
}

TEST_CASE("parse_item_list uses the item count when the total is missing") {
  auto list = parse_item_list(R"({"Items":[{"Id":"a","Type":"Movie"}]})");
  REQUIRE(list.ok());
  CHECK(list.value().total == 1);
}

TEST_CASE("parse_item_list never reports a total below the items it holds") {
  // Jellyfin 12.4 sends TotalRecordCount 0 when enableTotalRecordCount=false.
  auto list = parse_item_list(R"({"Items":[{"Id":"a"},{"Id":"b"}],"TotalRecordCount":0})");
  REQUIRE(list.ok());
  CHECK(list.value().total == 2);

  auto page = parse_item_list(R"({"Items":[{"Id":"a"},{"Id":"b"}],"TotalRecordCount":0,"StartIndex":50})");
  REQUIRE(page.ok());
  CHECK(page.value().total == 52);

  auto real = parse_item_list(R"({"Items":[{"Id":"a"}],"TotalRecordCount":42})");
  CHECK(real.value().total == 42);
}

TEST_CASE("parse_item_list maps library types") {
  auto list = parse_item_list(
      R"({"Items":[{"Id":"l1","Type":"CollectionFolder","CollectionType":"movies"},{"Id":"x","Type":"Audio"}]})");
  REQUIRE(list.ok());
  CHECK(list.value().items[0].type == ItemType::Library);
  CHECK(list.value().items[0].collection_type == "movies");
  CHECK(list.value().items[1].type == ItemType::Other);
}

TEST_CASE("parse_item_list treats a home video as a playable movie") {
  auto list = parse_item_list(R"({"Items":[{"Id":"v1","Type":"Video"}]})");
  REQUIRE(list.ok());
  CHECK(list.value().items[0].type == ItemType::Movie);
}

TEST_CASE("parsers return a Parse error for bad input") {
  CHECK(parse_item_list("not json").error().kind == ErrorKind::Parse);
  CHECK(parse_item_list("[]").error().kind == ErrorKind::Parse);
  CHECK(parse_item_list(R"({"Other":1})").error().kind == ErrorKind::Parse);
  CHECK(parse_auth_result("").error().kind == ErrorKind::Parse);
  CHECK(parse_server_info("{").error().kind == ErrorKind::Parse);
}

TEST_CASE("parse_server_info") {
  auto info = parse_server_info(
      R"({"LocalAddress":"http://[::1]:8096","ServerName":"jellyPi","Version":"12.4.0","Id":"abc"})");
  REQUIRE(info.ok());
  CHECK(info.value().id == "abc");
  CHECK(info.value().name == "jellyPi");
  CHECK(info.value().version == "12.4.0");
  CHECK_FALSE(parse_server_info(R"({"ServerName":"x"})").ok());
}

TEST_CASE("parse_auth_result") {
  auto auth = parse_auth_result(fixtures::auth_json());
  REQUIRE(auth.ok());
  CHECK(auth.value().access_token == "tok");
  CHECK(auth.value().user.id == "u1");
  CHECK(auth.value().user.name == "admin");
  CHECK(auth.value().server_id == "srv");
  CHECK_FALSE(parse_auth_result(R"({"User":{"Id":"u1"}})").ok());       // no token
  CHECK_FALSE(parse_auth_result(R"({"AccessToken":"t"})").ok());        // no user
}

TEST_CASE("parse_quick_connect") {
  auto session = parse_quick_connect_session(R"({"Secret":"s","Code":"123456","Authenticated":false})");
  REQUIRE(session.ok());
  CHECK(session.value().secret == "s");
  CHECK(session.value().code == "123456");
  CHECK_FALSE(parse_quick_connect_session(R"({"Code":"1"})").ok());

  CHECK(parse_quick_connect_authenticated(R"({"Authenticated":true})").value());
  CHECK_FALSE(parse_quick_connect_authenticated(R"({"Authenticated":false})").value());
  CHECK_FALSE(parse_quick_connect_authenticated(R"({})").value());
}

TEST_CASE("parse_playback_info") {
  auto info = parse_playback_info(fixtures::playback_info_transcode_json());
  REQUIRE(info.ok());
  CHECK(info.value().play_session_id == "ps1");
  REQUIRE(info.value().sources.size() == 1);
  CHECK(info.value().sources[0].transcoding_url.find("/videos/m1/master.m3u8") == 0);

  CHECK_FALSE(parse_playback_info(R"({"MediaSources":[]})").ok());
  auto refused = parse_playback_info(R"({"ErrorCode":"NoCompatibleStream","MediaSources":[]})");
  REQUIRE_FALSE(refused.ok());
  CHECK(refused.error().message.find("NoCompatibleStream") != std::string::npos);
}

TEST_CASE("tick conversions") {
  CHECK(seconds_to_ticks(1) == 10'000'000);
  CHECK(seconds_to_ticks(90.5) == 905'000'000);
  CHECK(ticks_to_seconds(6'000'000'000) == doctest::Approx(600.0));
}
