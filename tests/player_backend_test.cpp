#include <doctest/doctest.h>

#include <cmath>
#include <cstdlib>
#include <vector>

#include "player/backend.h"
#include "player/demo_backend.h"

using namespace zerofin::player;

namespace {
StartParams params(double start_seconds = 0) {
  StartParams p;
  p.url = "x";
  p.start_seconds = start_seconds;
  return p;
}
}  // namespace

TEST_CASE("demo: position advances while playing") {
  DemoBackend b;
  b.start(params());
  b.poll(1000);
  const PlayerState s = b.poll(11000);
  CHECK(std::fabs(s.position_s - 10.0) < 0.01);
  CHECK(s.started);
  CHECK(s.duration_s == kDemoDurationSeconds);
}

TEST_CASE("demo: start offset") {
  DemoBackend b;
  b.start(params(30));
  CHECK(std::fabs(b.poll(0).position_s - 30.0) < 0.01);
}

TEST_CASE("demo: pause stops the clock") {
  DemoBackend b;
  b.start(params());
  b.poll(0);
  b.set_paused(true);
  const PlayerState a = b.poll(5000);
  const PlayerState c = b.poll(9000);
  CHECK(a.position_s == c.position_s);
  CHECK(c.paused);
}

TEST_CASE("demo: seek clamps at zero") {
  DemoBackend b;
  b.start(params(10));
  b.seek_relative(-100);
  CHECK(b.poll(0).position_s == 0);
}

TEST_CASE("demo: seek past the end ends the video") {
  DemoBackend b;
  b.start(params(10));
  b.seek_relative(1e9);
  CHECK(b.poll(0).ended == EndReason::Eof);
}

TEST_CASE("demo: render is rate limited") {
  DemoBackend b;
  b.start(params());
  std::vector<uint8_t> buf(320 * 170 * 2);
  b.poll(0);
  CHECK(b.render(buf.data(), 640));
  b.poll(10);
  CHECK_FALSE(b.render(buf.data(), 640));
  b.poll(50);
  CHECK(b.render(buf.data(), 640));
}

TEST_CASE("factory: the demo flag returns a backend") {
  CHECK(make_backend(true) != nullptr);
}

TEST_CASE("demo: ZEROFIN_DEMO_DURATION shortens the video") {
  setenv("ZEROFIN_DEMO_DURATION", "100", 1);
  DemoBackend b;
  b.start(params(90));
  CHECK(b.poll(0).duration_s == 100.0);
  CHECK(b.poll(11000).ended == EndReason::Eof);
  unsetenv("ZEROFIN_DEMO_DURATION");
  DemoBackend d;
  d.start(params());
  CHECK(d.poll(0).duration_s == kDemoDurationSeconds);
}

TEST_CASE("factory: the demo backend ignores the output and the sound options") {
  BackendOptions o;
  o.output = Output::Hdmi;
  o.audio = AudioSink::Hdmi;
  auto b = make_backend(true, o);
  REQUIRE(b != nullptr);
  b->set_audio_sink(AudioSink::Default);  // must not crash
  b->start(params());
  CHECK(b->poll(0).started);  // it plays as usual: the options change nothing
}
