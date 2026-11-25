#pragma once

#include <string>

#include "jellyfin/models.h"

namespace zerofin::ui {

struct CardText {
  std::string caption;
  std::string badge;      // "S2E3" on an episode
  double progress = -1;   // 0..1 for a partly watched item, else -1
  bool watched = false;
};

// Episode: the caption is the series name and the badge is "S2E3" (or "E3" without a season).
// Other items: the caption is the name.
CardText describe_card(const jf::Item& item);

enum class TileKind { Movies, TvShows, Collections, Other, Search, Settings };
TileKind tile_kind(const jf::Item& library);

// The first version shows movies, TV and collections. Other library types are left out.
bool library_supported(const jf::Item& library);
// Libraries that get a "Recent in ..." row on the home screen.
bool has_latest_row(const jf::Item& library);

}  // namespace zerofin::ui
