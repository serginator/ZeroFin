#include <doctest/doctest.h>

#include "ui_core/playlist.h"

using namespace zerofin;

namespace {
std::vector<jf::Item> three() {
  std::vector<jf::Item> v(3);
  v[0].id = "a";
  v[1].id = "b";
  v[2].id = "c";
  return v;
}
}  // namespace

TEST_CASE("adjacent_episode finds the neighbours in list order") {
  const auto list = three();
  CHECK(ui::adjacent_episode(list, "b", 1)->id == "c");
  CHECK(ui::adjacent_episode(list, "b", -1)->id == "a");
}

TEST_CASE("adjacent_episode is null at the ends, for an empty list and for an unknown id") {
  const auto list = three();
  CHECK(ui::adjacent_episode(list, "a", -1) == nullptr);
  CHECK(ui::adjacent_episode(list, "c", 1) == nullptr);
  CHECK(ui::adjacent_episode({}, "a", 1) == nullptr);
  CHECK(ui::adjacent_episode(list, "zzz", 1) == nullptr);
}

TEST_CASE("track_ordinal is the 1-based place among the streams of one kind") {
  jf::MediaStream a, b;
  a.index = 1;
  b.index = 2;
  const std::vector<const jf::MediaStream*> streams = {&a, &b};
  CHECK(ui::track_ordinal(streams, 1) == 1);
  CHECK(ui::track_ordinal(streams, 2) == 2);
  CHECK_FALSE(ui::track_ordinal(streams, 9).has_value());
  CHECK_FALSE(ui::track_ordinal({}, 1).has_value());
}
