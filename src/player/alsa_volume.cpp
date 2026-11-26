#include "player/alsa_volume.h"

#include <alsa/asoundlib.h>

#include <cstdio>
#include <string>
#include <vector>

namespace zerofin::player {
namespace {

// The output controls to look for, best first. PipeWire and PulseAudio show "Master".
constexpr const char* kControlNames[] = {"Master", "PCM", "Speaker", "Headphone", "Digital"};

class AlsaVolume : public SystemVolume {
 public:
  AlsaVolume(snd_mixer_t* mixer, snd_mixer_elem_t* elem) : mixer_(mixer), elem_(elem) {
    snd_mixer_selem_get_playback_volume_range(elem_, &raw_min_, &raw_max_);
  }

  ~AlsaVolume() override { snd_mixer_close(mixer_); }

  AlsaVolume(const AlsaVolume&) = delete;
  AlsaVolume& operator=(const AlsaVolume&) = delete;

  std::optional<int> volume() override {
    for (int ch = 0; ch <= SND_MIXER_SCHN_LAST; ++ch) {
      const auto channel = static_cast<snd_mixer_selem_channel_id_t>(ch);
      if (!snd_mixer_selem_has_playback_channel(elem_, channel)) continue;
      long raw = 0;
      if (snd_mixer_selem_get_playback_volume(elem_, channel, &raw) < 0) return std::nullopt;
      return raw_to_percent(raw, raw_min_, raw_max_);
    }
    return std::nullopt;
  }

  bool set_volume(int percent) override {
    return snd_mixer_selem_set_playback_volume_all(elem_, percent_to_raw(percent, raw_min_, raw_max_)) >= 0;
  }

  std::optional<bool> muted() override {
    if (!snd_mixer_selem_has_playback_switch(elem_)) return std::nullopt;
    for (int ch = 0; ch <= SND_MIXER_SCHN_LAST; ++ch) {
      const auto channel = static_cast<snd_mixer_selem_channel_id_t>(ch);
      if (!snd_mixer_selem_has_playback_channel(elem_, channel)) continue;
      int on = 1;
      if (snd_mixer_selem_get_playback_switch(elem_, channel, &on) < 0) return std::nullopt;
      return on == 0;
    }
    return std::nullopt;
  }

  bool set_muted(bool muted) override {
    if (!snd_mixer_selem_has_playback_switch(elem_)) return false;
    return snd_mixer_selem_set_playback_switch_all(elem_, muted ? 0 : 1) >= 0;
  }

 private:
  snd_mixer_t* mixer_;
  snd_mixer_elem_t* elem_;
  long raw_min_ = 0;
  long raw_max_ = 0;
};

// The best output volume control of the mixer, or nullptr.
snd_mixer_elem_t* find_control(snd_mixer_t* mixer) {
  for (const char* wanted : kControlNames) {
    for (snd_mixer_elem_t* e = snd_mixer_first_elem(mixer); e; e = snd_mixer_elem_next(e)) {
      if (!snd_mixer_selem_is_active(e) || !snd_mixer_selem_has_playback_volume(e)) continue;
      if (std::string(snd_mixer_selem_get_name(e)) == wanted) return e;
    }
  }
  return nullptr;
}

std::unique_ptr<SystemVolume> open_device(const std::string& name) {
  snd_mixer_t* mixer = nullptr;
  if (snd_mixer_open(&mixer, 0) < 0) return nullptr;
  if (snd_mixer_attach(mixer, name.c_str()) < 0 || snd_mixer_selem_register(mixer, nullptr, nullptr) < 0 ||
      snd_mixer_load(mixer) < 0) {
    snd_mixer_close(mixer);
    return nullptr;
  }
  snd_mixer_elem_t* elem = find_control(mixer);
  if (!elem) {
    snd_mixer_close(mixer);
    return nullptr;
  }
  return std::make_unique<AlsaVolume>(mixer, elem);
}

}  // namespace

std::unique_ptr<SystemVolume> open_alsa_volume() {
  std::vector<std::string> devices = {"default"};
  for (int card = -1; snd_card_next(&card) >= 0 && card >= 0;) devices.push_back("hw:" + std::to_string(card));
  for (const std::string& device : devices) {
    if (auto volume = open_device(device)) return volume;
  }
  std::fprintf(stderr, "volume: no ALSA mixer control found, using the player volume\n");
  return nullptr;
}

}  // namespace zerofin::player
