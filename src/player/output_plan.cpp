#include "player/output_plan.h"

namespace zerofin::player {

BackendOptions plan_output(OutputSetting setting, const std::optional<HdmiConnector>& connected,
                           bool monitor_has_audio, bool sound_on_speaker) {
  BackendOptions o;
  o.output = choose_output(setting, connected.has_value());
  if (o.output == Output::Hdmi) {
    o.hdmi = *connected;
    o.audio = (monitor_has_audio && !sound_on_speaker) ? AudioSink::Hdmi : AudioSink::Default;
  }
  return o;
}

jf::Target target_for(Output output, std::string_view quality) {
  if (output == Output::Lcd) return jf::Target::Lcd;
  return quality == "1080" ? jf::Target::Hdmi1080 : jf::Target::Hdmi720;
}

}  // namespace zerofin::player
