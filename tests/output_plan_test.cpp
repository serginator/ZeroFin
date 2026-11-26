#include <doctest/doctest.h>

#include "player/output_plan.h"

using namespace zerofin::player;
using zerofin::jf::Target;

namespace {
const HdmiConnector kMonitor{"/dev/dri/card0", "HDMI-A-1"};
}

TEST_CASE("plan: no monitor means the LCD and the default sound") {
  const BackendOptions o = plan_output(OutputSetting::Auto, std::nullopt, true, false);
  CHECK(o.output == Output::Lcd);
  CHECK(o.audio == AudioSink::Default);
}

TEST_CASE("plan: a monitor in Auto mode gets the video and the sound") {
  const BackendOptions o = plan_output(OutputSetting::Auto, kMonitor, true, false);
  CHECK(o.output == Output::Hdmi);
  CHECK(o.hdmi.device == "/dev/dri/card0");
  CHECK(o.hdmi.connector == "HDMI-A-1");
  CHECK(o.audio == AudioSink::Hdmi);
}

TEST_CASE("plan: LCD only ignores the monitor") {
  const BackendOptions o = plan_output(OutputSetting::LcdOnly, kMonitor, true, false);
  CHECK(o.output == Output::Lcd);
  CHECK(o.audio == AudioSink::Default);
}

TEST_CASE("plan: the user can keep the sound on the CP0 speaker") {
  const BackendOptions o = plan_output(OutputSetting::Auto, kMonitor, true, true);
  CHECK(o.output == Output::Hdmi);
  CHECK(o.audio == AudioSink::Default);
}

TEST_CASE("plan: a monitor with no audio at all uses the CP0 speaker") {
  const BackendOptions o = plan_output(OutputSetting::Auto, kMonitor, false, false);
  CHECK(o.output == Output::Hdmi);
  CHECK(o.audio == AudioSink::Default);
}

TEST_CASE("target: the LCD uses the LCD profile whatever the quality is") {
  CHECK(target_for(Output::Lcd, "1080") == Target::Lcd);
  CHECK(target_for(Output::Lcd, "720") == Target::Lcd);
}

TEST_CASE("target: a monitor uses the chosen quality, 720p when unknown") {
  CHECK(target_for(Output::Hdmi, "720") == Target::Hdmi720);
  CHECK(target_for(Output::Hdmi, "1080") == Target::Hdmi1080);
  CHECK(target_for(Output::Hdmi, "") == Target::Hdmi720);
  CHECK(target_for(Output::Hdmi, "4k") == Target::Hdmi720);
}
