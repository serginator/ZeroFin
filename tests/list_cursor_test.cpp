#include <doctest/doctest.h>

#include "ui_core/list_cursor.h"

using namespace zerofin::ui;

namespace {
// Rows: 0 header, 1 a, 2 b, 3 header, 4 c, 5 d, 6 e
const std::vector<bool> kRows = {false, true, true, false, true, true, true};
}  // namespace

TEST_CASE("the first selectable row is selected") {
  ListCursor c(4);
  c.set_rows(kRows);
  CHECK(c.selected() == 1);
  CHECK(c.top() == 0);
}

TEST_CASE("move skips headers and scrolls") {
  ListCursor c(4);
  c.set_rows(kRows);
  CHECK(c.move(1));
  CHECK(c.selected() == 2);
  CHECK(c.move(1));
  CHECK(c.selected() == 4);  // header 3 skipped
  CHECK(c.top() == 1);       // row 4 would be below the 4 visible rows 0..3
}

TEST_CASE("moving up shows the header above the first row of a section") {
  ListCursor c(4);
  c.set_rows(kRows);
  c.select(6);
  CHECK(c.top() == 3);
  CHECK(c.move(-1));
  CHECK(c.selected() == 5);
  CHECK(c.move(-1));
  CHECK(c.selected() == 4);
  CHECK(c.top() == 3);  // header row 3 is shown
  CHECK(c.move(-1));
  CHECK(c.selected() == 2);
  CHECK(c.move(-1));
  CHECK(c.selected() == 1);
  CHECK(c.top() == 0);  // header row 0 is shown
}

TEST_CASE("move stops at both ends") {
  ListCursor c(4);
  c.set_rows(kRows);
  CHECK_FALSE(c.move(-1));
  c.select_last();
  CHECK(c.selected() == 6);
  CHECK_FALSE(c.move(1));
}

TEST_CASE("page moves about one screen") {
  ListCursor c(4);
  c.set_rows(kRows);
  CHECK(c.page(1));
  CHECK(c.selected() == 5);
  CHECK(c.page(-1));
  CHECK(c.selected() == 1);
  CHECK_FALSE(c.page(-1));
}

TEST_CASE("select_first and select_last") {
  ListCursor c(4);
  c.set_rows(kRows);
  CHECK(c.select_last());
  CHECK(c.selected() == 6);
  CHECK(c.select_first());
  CHECK(c.selected() == 1);
  CHECK(c.top() == 0);
  CHECK_FALSE(c.select_first());
}

TEST_CASE("select refuses headers and out-of-range rows") {
  ListCursor c(4);
  c.set_rows(kRows);
  CHECK_FALSE(c.select(3));
  CHECK_FALSE(c.select(99));
  CHECK(c.selected() == 1);
  CHECK(c.select(5));
  CHECK(c.selected() == 5);
}

TEST_CASE("an empty or header-only list has no selection") {
  ListCursor c(4);
  c.set_rows({});
  CHECK(c.selected() == -1);
  CHECK_FALSE(c.move(1));
  c.set_rows({false, false});
  CHECK(c.selected() == -1);
  CHECK_FALSE(c.page(1));
}

TEST_CASE("set_rows keeps the selection when the row is still selectable") {
  ListCursor c(4);
  c.set_rows(kRows);
  c.select(5);
  auto more = kRows;
  more.push_back(true);
  more.push_back(true);
  c.set_rows(more);
  CHECK(c.selected() == 5);
}

TEST_CASE("set_rows resets the selection when the list shrinks below it") {
  ListCursor c(4);
  c.set_rows(kRows);
  c.select(6);
  c.set_rows({false, true});
  CHECK(c.selected() == 1);
  CHECK(c.top() == 0);
}

TEST_CASE("a short list never scrolls") {
  ListCursor c(6);
  c.set_rows({true, true, true});
  c.select_last();
  CHECK(c.top() == 0);
}
