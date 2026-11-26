#include <doctest/doctest.h>

#include "ui_core/output_watch.h"

using namespace zerofin::ui;

TEST_CASE("watch: nothing changes while the output is right") {
  OutputWatch w;
  for (int i = 0; i < 5; ++i) {
    CHECK(w.check(false, false) == OutputChange::None);  // LCD, and the LCD is wanted
    CHECK(w.check(true, true) == OutputChange::None);    // monitor, and the monitor is wanted
  }
}

TEST_CASE("watch: a monitor that stays plugged in gets the video after two checks") {
  OutputWatch w;
  CHECK(w.check(false, true) == OutputChange::None);  // seen once: the monitor may still be starting
  CHECK(w.check(false, true) == OutputChange::ToMonitor);
}

TEST_CASE("watch: a monitor seen once and then gone does not move the video") {
  OutputWatch w;
  CHECK(w.check(false, true) == OutputChange::None);
  CHECK(w.check(false, false) == OutputChange::None);  // unplugged again: the count starts over
  CHECK(w.check(false, true) == OutputChange::None);
  CHECK(w.check(false, true) == OutputChange::ToMonitor);
}

TEST_CASE("watch: a pulled cable moves the video to the LCD at once") {
  OutputWatch w;
  CHECK(w.check(true, false) == OutputChange::ToLcd);
}

TEST_CASE("watch: after a change the count starts over") {
  OutputWatch w;
  w.check(false, true);
  CHECK(w.check(false, true) == OutputChange::ToMonitor);
  // The caller moved the video. It now plays on the monitor and the monitor is still wanted.
  CHECK(w.check(true, true) == OutputChange::None);
  // And if it ends up on the LCD again (a failure), it needs two new checks before it goes back.
  CHECK(w.check(false, true) == OutputChange::None);
  CHECK(w.check(false, true) == OutputChange::ToMonitor);
}
