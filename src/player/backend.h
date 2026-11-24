#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace zerofin::player {

struct StartParams {
  std::string url;                          // holds the token. Never log it without jf::redact_url().
  double start_seconds = 0;
  std::optional<std::string> subtitle_url;  // text subtitle file, drawn by the backend
  int volume = 100;                         // 0 to 100
  bool hardware_decode = true;              // direct play only
  // Direct play only. The server sends the whole file, so the backend picks the tracks.
  std::optional<int> audio_track;           // 1-based libmpv audio track number. None = the default.
  bool disable_embedded_subtitles = false;  // text subtitles come from subtitle_url instead
};

enum class EndReason { None, Eof, Error };

struct PlayerState {
  bool started = false;  // the first frame was drawn
  bool paused = false;
  bool buffering = false;
  double position_s = 0;
  double duration_s = 0;  // 0 when unknown
  EndReason ended = EndReason::None;
};

// Plays one stream and draws frames of 320x170 pixels in RGB565. The libmpv backend and the demo
// backend implement it. Everything runs on the UI thread.
class PlayerBackend {
 public:
  virtual ~PlayerBackend() = default;

  // May be called again to load another stream.
  virtual bool start(const StartParams& params) = 0;
  // Drains events and returns the state. `now_ms` is a monotonic clock in milliseconds.
  virtual PlayerState poll(uint32_t now_ms) = 0;
  // True when a new frame was written into `rgb565`.
  virtual bool render(uint8_t* rgb565, size_t stride) = 0;
  virtual void set_paused(bool paused) = 0;
  virtual void seek_relative(double seconds) = 0;
  virtual void set_volume(int percent) = 0;
  // Frees the decoder memory (CMA on the device).
  virtual void stop() = 0;
  // Redacted text of the last error.
  virtual const std::string& last_error() const = 0;
};

// The demo backend when `demo` is true, else the libmpv backend, else nullptr (a build without libmpv).
std::unique_ptr<PlayerBackend> make_backend(bool demo);

}  // namespace zerofin::player
