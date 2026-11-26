#include <doctest/doctest.h>

#include <memory>

#include "player/system_volume.h"
#include "player/volume.h"

using namespace zerofin::player;

namespace {

// A device mixer in memory. The tests read the state through the pointers they keep.
struct FakeState {
  int percent = 20;
  bool muted = false;
  bool readable = true;
  bool has_switch = true;
};

class FakeVolume : public SystemVolume {
 public:
  explicit FakeVolume(FakeState* s) : s_(s) {}
  std::optional<int> volume() override { return s_->readable ? std::optional<int>(s_->percent) : std::nullopt; }
  bool set_volume(int percent) override {
    s_->percent = percent;
    return true;
  }
  std::optional<bool> muted() override { return s_->has_switch ? std::optional<bool>(s_->muted) : std::nullopt; }
  bool set_muted(bool m) override {
    if (!s_->has_switch) return false;
    s_->muted = m;
    return true;
  }

 private:
  FakeState* s_;
};

Volume with_device(FakeState& s, int software_level = 100) {
  return Volume(std::make_unique<FakeVolume>(&s), software_level);
}

}  // namespace

TEST_CASE("raw range: percent and raw convert both ways") {
  CHECK(percent_to_raw(0, 0, 255) == 0);
  CHECK(percent_to_raw(100, 0, 255) == 255);
  CHECK(percent_to_raw(50, 0, 65536) == 32768);
  CHECK(percent_to_raw(150, 0, 255) == 255);   // clamped
  CHECK(percent_to_raw(-5, 10, 20) == 10);     // clamped, with a non-zero minimum
  CHECK(raw_to_percent(13107, 0, 65536) == 20);
  CHECK(raw_to_percent(0, 0, 255) == 0);
  CHECK(raw_to_percent(255, 0, 255) == 100);
  CHECK(raw_to_percent(5, 5, 5) == 0);         // an empty range
  for (int p = 0; p <= 100; ++p) {
    CHECK(raw_to_percent(percent_to_raw(p, 0, 255), 0, 255) == p);
    CHECK(raw_to_percent(percent_to_raw(p, 0, 65536), 0, 65536) == p);
  }
}

TEST_CASE("volume: starts at the device level, not at 100") {
  FakeState s;
  s.percent = 20;
  Volume v = with_device(s);
  CHECK(v.uses_system());
  CHECK(v.level() == 20);
  CHECK(v.backend_percent() == 100);  // the player stays at full volume: the device does the work
}

TEST_CASE("volume: the keys change the device volume") {
  FakeState s;
  s.percent = 20;
  Volume v = with_device(s);
  v.change(5);
  CHECK(s.percent == 25);
  v.change(-10);
  CHECK(s.percent == 15);
  v.change(-100);
  CHECK(s.percent == 0);
  v.change(500);
  CHECK(s.percent == 100);
}

TEST_CASE("volume: mute uses the device switch and keeps the level") {
  FakeState s;
  s.percent = 40;
  Volume v = with_device(s);
  v.toggle_mute();
  CHECK(v.muted());
  CHECK(s.muted);
  CHECK(s.percent == 40);
  v.toggle_mute();
  CHECK_FALSE(v.muted());
  CHECK_FALSE(s.muted);
}

TEST_CASE("volume: a volume key ends the mute") {
  FakeState s;
  Volume v = with_device(s);
  v.toggle_mute();
  v.change(5);
  CHECK_FALSE(v.muted());
  CHECK_FALSE(s.muted);
  CHECK(s.percent == 25);
}

TEST_CASE("volume: a device that is already muted starts muted") {
  FakeState s;
  s.muted = true;
  Volume v = with_device(s);
  CHECK(v.muted());
  v.toggle_mute();
  CHECK_FALSE(s.muted);
}

TEST_CASE("volume: no mute switch, so mute sets the device volume to 0") {
  FakeState s;
  s.percent = 40;
  s.has_switch = false;
  Volume v = with_device(s);
  v.toggle_mute();
  CHECK(s.percent == 0);
  CHECK(v.level() == 40);
  v.toggle_mute();
  CHECK(s.percent == 40);
}

TEST_CASE("volume: a mixer that cannot be read is dropped, the player volume is used") {
  FakeState s;
  s.readable = false;
  Volume v = with_device(s, 60);
  CHECK_FALSE(v.uses_system());
  CHECK(v.level() == 60);
  CHECK(v.backend_percent() == 60);
  v.change(5);
  CHECK(s.percent == 20);  // the device was not touched
}

TEST_CASE("volume: with no mixer the player volume does the work") {
  Volume v(nullptr, 100);
  CHECK_FALSE(v.uses_system());
  CHECK(v.backend_percent() == 100);
  v.change(-5);
  CHECK(v.level() == 95);
  CHECK(v.backend_percent() == 95);
  v.toggle_mute();
  CHECK(v.backend_percent() == 0);
  CHECK(v.level() == 95);
  v.toggle_mute();
  CHECK(v.backend_percent() == 95);
}
