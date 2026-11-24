#include <doctest/doctest.h>

#include "jellyfin/resume.h"

using namespace zerofin::jf;

namespace {

constexpr int64_t kHour = 3600LL * kTicksPerSecond;

UserData at(double seconds, bool played = false) {
  UserData d;
  d.position_ticks = seconds_to_ticks(seconds);
  d.played = played;
  return d;
}

}  // namespace

TEST_CASE("resume steps back 5 s from the saved position") {
  CHECK(resume_start_ticks(at(600), kHour) == seconds_to_ticks(595));
}

TEST_CASE("positions under 30 s start from the beginning") {
  CHECK(resume_start_ticks(at(0), kHour) == 0);
  CHECK(resume_start_ticks(at(29.9), kHour) == 0);
  CHECK(resume_start_ticks(at(30), kHour) == seconds_to_ticks(25));
}

TEST_CASE("positions past 95 percent start from the beginning") {
  CHECK(resume_start_ticks(at(3500), kHour) == 0);            // 97.2 %
  CHECK(resume_start_ticks(at(3420), kHour) == seconds_to_ticks(3415));  // exactly 95 %
}

TEST_CASE("a played item starts from the beginning") {
  CHECK(resume_start_ticks(at(600, true), kHour) == 0);
}

TEST_CASE("an unknown runtime still resumes") {
  CHECK(resume_start_ticks(at(600), 0) == seconds_to_ticks(595));
}

TEST_CASE("custom rules change the result") {
  ResumeRules rules;
  rules.min_resume_seconds = 5;
  rules.rewind_seconds = 0;
  CHECK(resume_start_ticks(at(10), kHour, rules) == seconds_to_ticks(10));
}

TEST_CASE("watched position starts at 90 percent") {
  CHECK_FALSE(is_watched_position(seconds_to_ticks(3000), kHour));
  CHECK(is_watched_position(seconds_to_ticks(3240), kHour));
  CHECK(is_watched_position(seconds_to_ticks(3599), kHour));
  CHECK_FALSE(is_watched_position(seconds_to_ticks(3599), 0));  // unknown runtime
}

TEST_CASE("progress_fraction is clamped to 0 - 1") {
  CHECK(progress_fraction(0, kHour) == 0.0);
  CHECK(progress_fraction(kHour / 2, kHour) == doctest::Approx(0.5));
  CHECK(progress_fraction(kHour * 2, kHour) == 1.0);
  CHECK(progress_fraction(100, 0) == 0.0);
  CHECK(progress_fraction(-5, kHour) == 0.0);
}
