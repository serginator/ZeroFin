#include "ui_core/cards.h"

#include <algorithm>

#include "jellyfin/resume.h"
#include "ui_core/format.h"

namespace zerofin::ui {

CardText describe_card(const jf::Item& item) {
  CardText t;
  t.caption = item.name;
  if (item.type == jf::ItemType::Episode) {
    if (!item.series_name.empty()) t.caption = item.series_name;
    t.badge = episode_label(item);
  }
  t.watched = item.user_data.played;
  if (!item.user_data.played && item.user_data.position_ticks > 0 && item.runtime_ticks > 0) {
    t.progress = std::max(0.01, jf::progress_fraction(item.user_data.position_ticks, item.runtime_ticks));
  }
  return t;
}

TileKind tile_kind(const jf::Item& library) {
  const std::string& t = library.collection_type;
  if (t == "movies") return TileKind::Movies;
  if (t == "tvshows") return TileKind::TvShows;
  if (t == "boxsets") return TileKind::Collections;
  return TileKind::Other;
}

bool library_supported(const jf::Item& library) {
  const std::string& t = library.collection_type;
  return t.empty() || t == "movies" || t == "tvshows" || t == "boxsets" || t == "homevideos" || t == "mixed";
}

bool has_latest_row(const jf::Item& library) { return tile_kind(library) != TileKind::Other; }

}  // namespace zerofin::ui
