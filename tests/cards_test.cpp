#include <doctest/doctest.h>

#include "ui_core/cards.h"

using namespace zerofin;

namespace {
jf::Item episode(int season, int number) {
  jf::Item e;
  e.type = jf::ItemType::Episode;
  e.name = "Pilot";
  e.series_name = "Show";
  e.parent_index_number = season;
  e.index_number = number;
  return e;
}
jf::Item library(const char* type) {
  jf::Item l;
  l.type = jf::ItemType::Library;
  l.collection_type = type;
  return l;
}
}  // namespace

TEST_CASE("an episode shows the series name and S2E3") {
  const auto t = ui::describe_card(episode(2, 3));
  CHECK(t.caption == "Show");
  CHECK(t.badge == "S2E3");
}

TEST_CASE("an episode without a season shows E3") {
  CHECK(ui::describe_card(episode(-1, 3)).badge == "E3");
}

TEST_CASE("a movie shows its name and no badge") {
  jf::Item m;
  m.type = jf::ItemType::Movie;
  m.name = "Heat";
  const auto t = ui::describe_card(m);
  CHECK(t.caption == "Heat");
  CHECK(t.badge.empty());
  CHECK(t.progress < 0);
}

TEST_CASE("progress is a fraction for a partly watched item") {
  jf::Item m;
  m.type = jf::ItemType::Movie;
  m.runtime_ticks = 1000;
  m.user_data.position_ticks = 250;
  CHECK(ui::describe_card(m).progress == doctest::Approx(0.25));
}

TEST_CASE("a played item has no progress and is watched") {
  jf::Item m;
  m.type = jf::ItemType::Movie;
  m.runtime_ticks = 1000;
  m.user_data.position_ticks = 250;
  m.user_data.played = true;
  const auto t = ui::describe_card(m);
  CHECK(t.progress < 0);
  CHECK(t.watched);
}

TEST_CASE("tile_kind maps the collection types") {
  CHECK(ui::tile_kind(library("movies")) == ui::TileKind::Movies);
  CHECK(ui::tile_kind(library("tvshows")) == ui::TileKind::TvShows);
  CHECK(ui::tile_kind(library("boxsets")) == ui::TileKind::Collections);
  CHECK(ui::tile_kind(library("music")) == ui::TileKind::Other);
}

TEST_CASE("library_supported rejects livetv and playlists") {
  CHECK(ui::library_supported(library("movies")));
  CHECK(ui::library_supported(library("")));
  CHECK(ui::library_supported(library("homevideos")));
  CHECK_FALSE(ui::library_supported(library("livetv")));
  CHECK_FALSE(ui::library_supported(library("playlists")));
}

TEST_CASE("has_latest_row is true for movies tvshows boxsets only") {
  CHECK(ui::has_latest_row(library("movies")));
  CHECK(ui::has_latest_row(library("tvshows")));
  CHECK(ui::has_latest_row(library("boxsets")));
  CHECK_FALSE(ui::has_latest_row(library("homevideos")));
  CHECK_FALSE(ui::has_latest_row(library("")));
}
