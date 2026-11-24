#include <doctest/doctest.h>

#include <cstdlib>
#include <memory>

#include "jellyfin/client.h"
#include "jellyfin/demo_http.h"
#include "jellyfin/url.h"

using namespace zerofin::jf;

namespace {

struct Demo {
  Demo() {
    ::setenv("ZEROFIN_DEMO_LATENCY_MS", "0", 1);
    http = std::make_unique<DemoHttpClient>();
    client = std::make_unique<Client>(*http, ClientInfo{"ZeroFin", "test", "dev", "0"}, "http://demo:8096");
  }
  Demo& login() {
    REQUIRE(client->login_password("demo", "demo").ok());
    return *this;
  }
  std::unique_ptr<DemoHttpClient> http;
  std::unique_ptr<Client> client;
};

}  // namespace

TEST_CASE("the demo server reports itself") {
  Demo d;
  auto info = d.client->public_info();
  REQUIRE(info.ok());
  CHECK(info.value().name == "Demo Server");
}

TEST_CASE("demo login: Quick Connect approves after a few polls, password needs demo/demo") {
  Demo d;
  auto qc = d.client->quick_connect_initiate();
  REQUIRE(qc.ok());
  CHECK(qc.value().code.size() == 6);
  CHECK_FALSE(d.client->quick_connect_authorized(qc.value().secret).value());
  CHECK_FALSE(d.client->quick_connect_authorized(qc.value().secret).value());
  CHECK(d.client->quick_connect_authorized(qc.value().secret).value());
  CHECK(d.client->login_quick_connect(qc.value().secret).ok());

  Demo e;
  CHECK(e.client->login_password("demo", "wrong").error().kind == ErrorKind::Unauthorized);
  CHECK(e.client->login_password("demo", "demo").ok());
}

TEST_CASE("demo home data: views, resume and next up") {
  Demo d;
  d.login();
  CHECK(d.client->views().value().items.size() == 5);
  auto resume = d.client->resume_items();
  REQUIRE(resume.ok());
  CHECK(resume.value().items.size() == 2);
  CHECK(resume.value().total == 2);  // the server sends 0, the parser fixes it
  CHECK(resume.value().items[0].user_data.position_ticks > 0);
  CHECK(d.client->next_up().value().items.size() == 3);
  for (const auto& it : d.client->next_up().value().items) CHECK_FALSE(it.user_data.played);
}

TEST_CASE("demo library paging") {
  Demo d;
  d.login();
  ItemQuery q;
  q.parent_id = "lib-movies";
  q.include_types = "Movie";
  q.limit = 15;
  auto first = d.client->items(q);
  REQUIRE(first.ok());
  CHECK(first.value().items.size() == 15);
  CHECK(first.value().total == 40);
  q.start_index = 30;
  auto last = d.client->items(q);
  CHECK(last.value().items.size() == 10);
  CHECK(last.value().start_index == 30);
}

TEST_CASE("demo series: seasons, episodes and search") {
  Demo d;
  d.login();
  auto seasons = d.client->seasons("s2");
  REQUIRE(seasons.ok());
  CHECK(seasons.value().items.size() == 3);
  CHECK(seasons.value().items[0].type == ItemType::Season);
  auto episodes = d.client->episodes("s2", "s2-se1");
  REQUIRE(episodes.ok());
  CHECK(episodes.value().items.size() == 8);
  CHECK(episodes.value().items[2].series_name == "Fleabag");

  auto hits = d.client->search("drag");
  REQUIRE(hits.ok());
  REQUIRE_FALSE(hits.value().items.empty());
  CHECK(hits.value().items[0].name == "Dragon Ball");
  CHECK(d.client->search("zzzz").value().items.empty());
}

TEST_CASE("demo playback: direct play for the Main 1080p file, transcode for the 8 Mbps one") {
  Demo d;
  d.login();

  auto direct_item = d.client->item("m4");  // variant 0: H.264 Main 1080p 5.2 Mbps
  REQUIRE(direct_item.ok());
  PlayRequest req;
  req.item_id = "m4";
  req.source = &direct_item.value().media_sources[0];
  auto direct = d.client->prepare_playback(req);
  REQUIRE(direct.ok());
  CHECK(direct.value().method == Method::DirectPlay);
  CHECK(direct.value().url.find("/Videos/m4/stream?static=true") != std::string::npos);

  auto heavy_item = d.client->item("m1");  // variant 1: H.264 High, 8.1 Mbps, PGS subtitle
  REQUIRE(heavy_item.ok());
  req.item_id = "m1";
  req.source = &heavy_item.value().media_sources[0];
  auto transcode = d.client->prepare_playback(req);
  REQUIRE(transcode.ok());
  CHECK(transcode.value().method == Method::Transcode);
  CHECK(get_query_param(transcode.value().url, "maxWidth") == "320");
  CHECK(get_query_param(transcode.value().url, "SubtitleStreamIndex") == "-1");
}

TEST_CASE("the demo server answers 404 for an unknown route") {
  Demo d;
  d.login();
  CHECK(d.client->item("nope").error().kind == ErrorKind::NotFound);
}
