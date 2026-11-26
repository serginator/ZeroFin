#include "player/mpv_backend.h"

#include <mpv/client.h>
#include <mpv/render.h>

#include <algorithm>
#include <utility>
#include <cstdio>

#include "jellyfin/url.h"
#include "player/mpv_options.h"

namespace zerofin::player {
namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 170;
constexpr uint32_t kPropsIntervalMs = 250;

}  // namespace

MpvBackend::~MpvBackend() { stop(); }

void MpvBackend::fail(const std::string& what) {
  std::fprintf(stderr, "player: %s\n", jf::redact_text(what).c_str());
}

bool MpvBackend::create_handle() {
  mpv_ = mpv_create();
  if (!mpv_) {
    fail("cannot create the player");
    return false;
  }
  for (const auto& kv : mpv_options(options_)) {
    const int rc = mpv_set_option_string(mpv_, kv.first.c_str(), kv.second.c_str());
    if (rc < 0) std::fprintf(stderr, "player: option %s rejected: %s\n", kv.first.c_str(), mpv_error_string(rc));
  }
  if (mpv_initialize(mpv_) < 0) {
    fail("cannot start the player");
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
    return false;
  }
  mpv_request_log_messages(mpv_, "warn");

  // On a monitor mpv draws by itself. There is no render context and no frame to copy.
  if (options_.output == Output::Hdmi) return true;

  mpv_render_param ctx_params[] = {
      {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_SW)},
      {MPV_RENDER_PARAM_INVALID, nullptr},
  };
  if (mpv_render_context_create(&render_, mpv_, ctx_params) < 0) {
    fail("cannot create the render context");
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
    render_ = nullptr;
    return false;
  }
  return true;
}

bool MpvBackend::start(const StartParams& params) {
  if (!mpv_ && !create_handle()) return false;

  state_ = PlayerState{};
  props_read_ = false;
  subtitle_url_ = params.subtitle_url;

  mpv_set_property_string(mpv_, "hwdec", params.hardware_decode ? "v4l2m2m-copy" : "no");
  set_volume(params.volume);
  mpv_set_property_string(mpv_, "pause", "no");

  // Per-file options: the start time and, for direct play, the chosen tracks.
  char start_option[96];
  int len = std::snprintf(start_option, sizeof(start_option), "start=%.3f", std::max(0.0, params.start_seconds));
  if (params.audio_track && *params.audio_track > 0 && len > 0 && len < 60) {
    len += std::snprintf(start_option + len, sizeof(start_option) - static_cast<size_t>(len), ",aid=%d", *params.audio_track);
  }
  if (params.disable_embedded_subtitles && len > 0 && len < 80) {
    std::snprintf(start_option + len, sizeof(start_option) - static_cast<size_t>(len), ",sid=no");
  }
  const char* cmd[] = {"loadfile", params.url.c_str(), "replace", "-1", start_option, nullptr};
  const int rc = mpv_command(mpv_, cmd);
  if (rc < 0) {
    fail(std::string("cannot load the stream: ") + mpv_error_string(rc));
    return false;
  }
  return true;
}

void MpvBackend::drain_events() {
  while (mpv_) {
    mpv_event* ev = mpv_wait_event(mpv_, 0);
    if (ev->event_id == MPV_EVENT_NONE) break;
    switch (ev->event_id) {
      case MPV_EVENT_LOG_MESSAGE: {
        auto* m = static_cast<mpv_event_log_message*>(ev->data);
        std::fprintf(stderr, "[mpv/%s] %s: %s", m->level, m->prefix, jf::redact_text(m->text).c_str());
        break;
      }
      case MPV_EVENT_PLAYBACK_RESTART:
        // The first picture is on the screen. On the LCD, render() says so. On a monitor nothing renders here.
        if (options_.output == Output::Hdmi) state_.started = true;
        break;
      case MPV_EVENT_FILE_LOADED:
        if (subtitle_url_) {
          const char* cmd[] = {"sub-add", subtitle_url_->c_str(), "select", nullptr};
          mpv_command(mpv_, cmd);
        }
        break;
      case MPV_EVENT_END_FILE: {
        auto* e = static_cast<mpv_event_end_file*>(ev->data);
        if (e->reason == MPV_END_FILE_REASON_EOF) {
          state_.ended = EndReason::Eof;
        } else if (e->reason == MPV_END_FILE_REASON_ERROR) {
          state_.ended = EndReason::Error;
          fail(std::string("playback error: ") + mpv_error_string(e->error));
        }
        // A stop (loading another stream with "replace") is not the end of the new stream.
        break;
      }
      case MPV_EVENT_SHUTDOWN:
        // The core quit. Stop reading events, so this loop cannot spin.
        if (state_.ended == EndReason::None) {
          state_.ended = EndReason::Error;
          fail("player quit");
        }
        return;
      default:
        break;
    }
  }
}

void MpvBackend::refresh_properties(uint32_t now_ms) {
  if (!mpv_) return;
  if (props_read_ && now_ms - last_props_ms_ < kPropsIntervalMs) return;
  props_read_ = true;
  last_props_ms_ = now_ms;

  double v = 0;
  if (mpv_get_property(mpv_, "time-pos", MPV_FORMAT_DOUBLE, &v) >= 0) state_.position_s = v;
  if (mpv_get_property(mpv_, "duration", MPV_FORMAT_DOUBLE, &v) >= 0) state_.duration_s = v;
  int flag = 0;
  if (mpv_get_property(mpv_, "pause", MPV_FORMAT_FLAG, &flag) >= 0) state_.paused = flag != 0;
  flag = 0;
  if (mpv_get_property(mpv_, "paused-for-cache", MPV_FORMAT_FLAG, &flag) >= 0) state_.buffering = flag != 0;
}

PlayerState MpvBackend::poll(uint32_t now_ms) {
  drain_events();
  refresh_properties(now_ms);
  return state_;
}

bool MpvBackend::render(uint8_t* rgb565, size_t stride) {
  if (!render_) return false;
  if (!(mpv_render_context_update(render_) & MPV_RENDER_UPDATE_FRAME)) return false;

  int size[2] = {kWidth, kHeight};
  size_t stride_bytes = stride;
  mpv_render_param params[] = {
      {MPV_RENDER_PARAM_SW_SIZE, size},
      {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char*>("rgb565")},
      {MPV_RENDER_PARAM_SW_STRIDE, &stride_bytes},
      {MPV_RENDER_PARAM_SW_POINTER, rgb565},
      {MPV_RENDER_PARAM_INVALID, nullptr},
  };
  const int rc = mpv_render_context_render(render_, params);
  if (rc < 0) {
    fail(std::string("render failed: ") + mpv_error_string(rc));
    state_.ended = EndReason::Error;
    return false;
  }
  state_.started = true;
  return true;
}

void MpvBackend::set_paused(bool paused) {
  if (!mpv_) return;
  mpv_set_property_string(mpv_, "pause", paused ? "yes" : "no");
  state_.paused = paused;
}

void MpvBackend::seek_relative(double seconds) {
  if (!mpv_) return;
  char amount[32];
  std::snprintf(amount, sizeof(amount), "%.3f", seconds);
  const char* cmd[] = {"seek", amount, "relative", nullptr};
  mpv_command(mpv_, cmd);
}

void MpvBackend::set_volume(int percent) {
  if (!mpv_) return;
  mpv_set_property_string(mpv_, "volume", std::to_string(std::clamp(percent, 0, 100)).c_str());
}

void MpvBackend::set_audio_sink(AudioSink sink) {
  if (options_.output != Output::Hdmi || !mpv_) return;
  mpv_set_property_string(mpv_, "audio-device", audio_device_name(sink).c_str());
}

void MpvBackend::stop() {
  // The render context goes first, then the player (same order as the spike).
  if (render_) {
    mpv_render_context_free(render_);
    render_ = nullptr;
  }
  if (mpv_) {
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
  }
}

}  // namespace zerofin::player
