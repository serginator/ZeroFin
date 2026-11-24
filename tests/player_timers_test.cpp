#include <doctest/doctest.h>

#include "ui_core/player_timers.h"

using namespace zerofin::ui;

TEST_CASE("report scheduler: every 10 s while playing") {
  ReportScheduler s;
  s.start(0);
  CHECK_FALSE(s.progress_due(9999, false));
  CHECK(s.progress_due(10000, false));
  CHECK_FALSE(s.progress_due(10001, false));
  CHECK(s.progress_due(20000, false));
}

TEST_CASE("report scheduler: a pause change reports at once") {
  ReportScheduler s;
  s.start(0);
  CHECK_FALSE(s.progress_due(1000, false));
  CHECK(s.progress_due(1500, true));
  CHECK_FALSE(s.progress_due(1600, true));
  CHECK(s.progress_due(1700, false));
}

TEST_CASE("countdown") {
  Countdown c;
  CHECK_FALSE(c.active());
  c.start(1000);
  CHECK(c.active());
  CHECK(c.seconds_left(1000) == 10);
  CHECK(c.seconds_left(1999) == 10);
  CHECK(c.seconds_left(2000) == 9);
  CHECK(c.seconds_left(10999) == 1);
  CHECK_FALSE(c.done(10999));
  CHECK(c.done(11000));
  CHECK(c.seconds_left(11000) == 0);
  c.cancel();
  CHECK_FALSE(c.active());
  CHECK_FALSE(c.done(20000));
}

TEST_CASE("reached_end trusts an end of file only near the end") {
  CHECK(reached_end(3595, 3600));
  CHECK(reached_end(3590, 3600));
  CHECK_FALSE(reached_end(3589.9, 3600));
  CHECK_FALSE(reached_end(2400, 3600));
  CHECK_FALSE(reached_end(0, 3600));
  CHECK(reached_end(100, 0));  // unknown length: trust the end of file
}

TEST_CASE("restart_position keeps the planned start until the first frame") {
  CHECK(restart_position(true, 30, 10) == 30);
  CHECK(restart_position(false, 0, 2400) == 2400);
  CHECK(restart_position(false, 0, 0) == 0);
}
