#include <doctest/doctest.h>

#include "ui_core/grid_cursor.h"

using namespace zerofin::ui;

TEST_CASE("a new cursor selects row 0 column 0") {
  GridCursor c;
  c.set_rows({{3, 2}, {5, 4}});
  CHECK(c.row() == 0);
  CHECK(c.col() == 0);
  CHECK_FALSE(c.empty());
}

TEST_CASE("down keeps the wanted column and clamps it") {
  GridCursor c;
  c.set_rows({{5, 4}, {2, 4}, {5, 4}});
  CHECK(c.select(0, 4));
  CHECK(c.move(Key::Down));
  CHECK(c.row() == 1);
  CHECK(c.col() == 1);
  CHECK(c.move(Key::Down));
  CHECK(c.row() == 2);
  CHECK(c.col() == 4);
  CHECK_FALSE(c.move(Key::Down));
}

TEST_CASE("up and down skip empty rows") {
  GridCursor c;
  c.set_rows({{2, 2}, {0, 1}, {2, 2}});
  CHECK(c.select(0, 1));
  CHECK(c.move(Key::Down));
  CHECK(c.row() == 2);
  CHECK(c.col() == 1);
  CHECK(c.move(Key::Up));
  CHECK(c.row() == 0);
}

TEST_CASE("left and right stop at the ends") {
  GridCursor c;
  c.set_rows({{3, 4}});
  CHECK_FALSE(c.move(Key::Left));
  CHECK(c.move(Key::Right));
  CHECK(c.move(Key::Right));
  CHECK(c.col() == 2);
  CHECK_FALSE(c.move(Key::Right));
}

TEST_CASE("the window follows the column") {
  GridCursor c;
  c.set_rows({{10, 4}});
  for (int i = 0; i < 5; ++i) CHECK(c.move(Key::Right));
  CHECK(c.col() == 5);
  CHECK(c.left(0) == 2);
  for (int i = 0; i < 5; ++i) CHECK(c.move(Key::Left));
  CHECK(c.col() == 0);
  CHECK(c.left(0) == 0);
}

TEST_CASE("a row keeps its window when the cursor leaves") {
  GridCursor c;
  c.set_rows({{10, 4}, {3, 4}});
  for (int i = 0; i < 6; ++i) c.move(Key::Right);
  CHECK(c.left(0) == 3);
  CHECK(c.move(Key::Down));
  CHECK(c.col() == 2);
  CHECK(c.move(Key::Up));
  CHECK(c.col() == 6);
  CHECK(c.left(0) == 3);
}

TEST_CASE("home end pageup pagedown") {
  GridCursor c;
  c.set_rows({{4, 4}, {0, 4}, {6, 4}, {5, 4}});
  CHECK(c.move(Key::End));
  CHECK(c.col() == 3);
  CHECK(c.move(Key::Home));
  CHECK(c.col() == 0);
  CHECK(c.move(Key::PageDown));
  CHECK(c.row() == 3);
  CHECK_FALSE(c.move(Key::PageDown));
  CHECK(c.move(Key::PageUp));
  CHECK(c.row() == 0);
  CHECK_FALSE(c.move(Key::PageUp));
}

TEST_CASE("set_rows keeps the position and clamps when rows shrink") {
  GridCursor c;
  c.set_rows({{5, 4}, {5, 4}});
  CHECK(c.select(1, 3));
  c.set_rows({{5, 4}, {5, 4}});
  CHECK(c.row() == 1);
  CHECK(c.col() == 3);
  c.set_rows({{5, 4}});
  CHECK(c.row() == 0);
  CHECK(c.col() == 3);
  c.set_rows({{2, 4}});
  CHECK(c.row() == 0);
  CHECK(c.col() == 1);
}

TEST_CASE("empty cursor ignores keys") {
  GridCursor c;
  c.set_rows({{0, 4}, {0, 4}});
  CHECK(c.empty());
  CHECK_FALSE(c.move(Key::Down));
  CHECK_FALSE(c.select(0, 0));
  c.set_rows({});
  CHECK(c.empty());
}

TEST_CASE("find_position prefers the same item") {
  const std::vector<RowKeys> rows = {{"resume", {"a", "b"}}, {"next", {"c", "d", "e"}}};
  auto p = find_position(rows, "next", "e", 0);
  REQUIRE(p.has_value());
  CHECK(p->first == 1);
  CHECK(p->second == 2);
}

TEST_CASE("find_position falls back to the same row and column") {
  const std::vector<RowKeys> rows = {{"resume", {"a", "b"}}, {"next", {"c", "d"}}};
  auto p = find_position(rows, "next", "gone", 5);
  REQUIRE(p.has_value());
  CHECK(p->first == 1);
  CHECK(p->second == 1);
}

TEST_CASE("find_position gives nullopt for an unknown row") {
  const std::vector<RowKeys> rows = {{"resume", {"a"}}};
  CHECK_FALSE(find_position(rows, "nope", "a", 0).has_value());
}
