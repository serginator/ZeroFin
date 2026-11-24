#include <algorithm>
#include <memory>
#include <optional>

#include "platform/platform.h"
#include "platform/status_reader.h"
#include "player/backend.h"
#include "screens/screens.h"
#include "ui/player_overlay.h"
#include "ui/widgets.h"
#include "ui_core/format.h"
#include "ui_core/player_controls.h"
#include "ui_core/player_timers.h"
#include "ui_core/playlist.h"

namespace zerofin::app {
namespace {

constexpr uint32_t kNetworkSlowMs = 5000;
constexpr uint32_t kStatusRefreshMs = 30000;
constexpr int kVolumeStep = 5;

// A new item with the plan to play it.
struct Loaded {
  jf::Item item;
  jf::PlaybackPlan plan;
};

// One row of the audio and subtitle menu.
struct MenuEntry {
  bool header = false;
  bool audio = false;
  int stream = -1;  // Jellyfin stream index. -1 = subtitles off.
};

std::optional<int> index_or_none(int index) { return index >= 0 ? std::optional<int>(index) : std::nullopt; }

class PlayerScreen : public Screen {
 public:
  PlayerScreen(AppContext& ctx, jf::PlaybackPlan plan, jf::Item item)
      : Screen(ctx, ""), plan_(std::move(plan)), item_(std::move(item)) {}

  ~PlayerScreen() override {
    // Free the decoder first. Then remove the canvas before the buffer it draws from.
    if (backend_) backend_->stop();
    if (canvas_) lv_obj_delete(canvas_);
    if (draw_buf_) lv_draw_buf_destroy(draw_buf_);
  }

  bool has_title_bar() const override { return false; }

  void build() override {
    lv_obj_set_style_bg_color(root_, lv_color_black(), 0);
    draw_buf_ = lv_draw_buf_create(kScreenW, kScreenH, LV_COLOR_FORMAT_RGB565, 0);
    canvas_ = lv_canvas_create(root_);
    if (draw_buf_) {
      lv_canvas_set_draw_buf(canvas_, draw_buf_);
      lv_canvas_fill_bg(canvas_, lv_color_black(), LV_OPA_COVER);
    }
    lv_obj_set_pos(canvas_, 0, 0);
    overlay_ = std::make_unique<PlayerOverlay>(root_, ctx_.fonts);
    controls_.set_neighbors(false, false);
    add_timer(8, [this] { tick(); });
  }

  void on_show() override {
    if (started_) return;
    started_ = true;
    backend_ = ctx_.make_backend ? ctx_.make_backend() : nullptr;
    if (!backend_) {
      ctx_.toast->show("No video player in this build", Toast::Kind::Error, 3500);
      leave();
      return;
    }
    start_stream();
    load_episode_list();
  }

  bool on_key(const ui::KeyEvent& raw) override {
    if (left_) return true;
    // F X Z C are the arrows and Q S D are media keys, with no fn needed. Every state below
    // (countdown, loading, track menu, controls) sees the translated key.
    const ui::KeyEvent e = ui::translate_player_key(raw);
    if (countdown_.active()) {
      if (e.key == ui::Key::Esc && !e.repeat) {
        countdown_.cancel();
        leave();
      }
      return true;
    }
    if (switching_) {  // a new stream is loading: only Esc works
      if (e.key == ui::Key::Esc && !e.repeat) leave();
      return true;
    }
    if (menu_open_) return on_menu_key(e);

    const uint32_t now = platform::tick_ms();
    const ui::PlayerAction action = controls_.on_key(e, now);
    switch (action) {
      case ui::PlayerAction::TogglePause:
        backend_->set_paused(!state_.paused);
        state_.paused = !state_.paused;
        break;
      case ui::PlayerAction::SeekBack:
        backend_->seek_relative(-ui::kSeekSeconds);
        overlay_->show_notice("-" + std::to_string(ui::kSeekSeconds) + " s");
        break;
      case ui::PlayerAction::SeekForward:
        backend_->seek_relative(ui::kSeekSeconds);
        overlay_->show_notice("+" + std::to_string(ui::kSeekSeconds) + " s");
        break;
      case ui::PlayerAction::ToggleMute:
        muted_ = !muted_;
        backend_->set_volume(muted_ ? 0 : ctx_.volume);
        if (muted_) {
          overlay_->show_notice("Muted");
        } else {
          overlay_->show_volume(ctx_.volume);
        }
        break;
      case ui::PlayerAction::VolumeUp:
      case ui::PlayerAction::VolumeDown: {
        const int step = action == ui::PlayerAction::VolumeUp ? kVolumeStep : -kVolumeStep;
        ctx_.volume = std::clamp(ctx_.volume + step, 0, 100);
        muted_ = false;  // changing the volume ends the mute
        backend_->set_volume(ctx_.volume);
        overlay_->show_volume(ctx_.volume);
        break;
      }
      case ui::PlayerAction::Previous:
        switch_to_neighbor(-1);
        break;
      case ui::PlayerAction::Next:
        switch_to_neighbor(1);
        break;
      case ui::PlayerAction::OpenTracks:
        open_track_menu();
        break;
      case ui::PlayerAction::Exit:
        leave();
        break;
      case ui::PlayerAction::None:
        break;
    }
    refresh_overlay(now);
    return true;
  }

 private:
  enum class Report { Start, Progress, Stopped };

  // ------------------------------------------------------------------ starting

  void start_stream() {
    state_ = player::PlayerState{};
    reported_start_ = false;
    buffering_since_.reset();
    player::StartParams params;
    params.url = plan_.url;
    params.start_seconds = jf::ticks_to_seconds(plan_.start_ticks);
    params.subtitle_url = plan_.external_subtitle_url;
    params.volume = muted_ ? 0 : ctx_.volume;
    params.hardware_decode = plan_.method == jf::Method::DirectPlay;
    if (plan_.method == jf::Method::DirectPlay && !item_.media_sources.empty()) {
      // Direct play sends the whole file, so the server ignores the track choice. Pick here.
      params.audio_track = ui::track_ordinal(item_.media_sources[0].audio_streams(), plan_.audio_index);
      params.disable_embedded_subtitles = true;
    }
    if (!backend_->start(params)) {
      ctx_.toast->show("Cannot start playback", Toast::Kind::Error, 3500);
      leave();
    }
  }

  // The episodes of the season, for the previous and next icons.
  void load_episode_list() {
    if (item_.type != jf::ItemType::Episode || item_.series_id.empty() || item_.season_id.empty()) return;
    auto client = ctx_.client;
    const std::string series = item_.series_id;
    const std::string season = item_.season_id;
    async([client, series, season] { return client->episodes(series, season); },
          [this](jf::Result<jf::ItemList> r) {
            episodes_.clear();
            if (r) episodes_ = std::move(r.value().items);  // a failed load leaves both icons off
            update_neighbors();
          });
  }

  void update_neighbors() {
    controls_.set_neighbors(ui::adjacent_episode(episodes_, item_.id, -1) != nullptr,
                            ui::adjacent_episode(episodes_, item_.id, 1) != nullptr);
  }

  // ----------------------------------------------------------------- switching

  void switch_to_neighbor(int direction) {
    const jf::Item* target = ui::adjacent_episode(episodes_, item_.id, direction);
    if (target) switch_to(target->id);
  }

  // Stops the old item and plays another one from the start.
  void switch_to(const std::string& id) {
    if (switching_ || left_) return;
    begin_switch("Loading...");
    auto client = ctx_.client;
    const auto mode = ctx_.play_mode();
    async(
        [client, id, mode]() -> jf::Result<Loaded> {
          auto item = client->item(id);
          if (!item) return item.error();
          Loaded loaded;
          loaded.item = std::move(item.value());
          if (loaded.item.media_sources.empty()) return jf::Error{jf::ErrorKind::NotFound, "No playable file", 0};
          const jf::MediaSource& src = loaded.item.media_sources[0];
          jf::PlayRequest req;
          req.item_id = loaded.item.id;
          req.source = &src;
          req.mode = mode;
          if (const jf::MediaStream* audio = src.default_audio()) req.audio_index = audio->index;
          auto plan = client->prepare_playback(req);
          if (!plan) return plan.error();
          loaded.plan = std::move(plan.value());
          return loaded;
        },
        [this](jf::Result<Loaded> r) {
          if (left_) return;
          if (!r) return fail_switch(r.error());
          item_ = std::move(r.value().item);
          plan_ = std::move(r.value().plan);
          retried_ = false;
          force_transcode_ = false;
          switching_ = false;
          start_stream();
          update_neighbors();
        });
  }

  // Plays the same item again with other tracks, or as a transcode, from the current position.
  // `banner` is the text shown while the new stream loads.
  void reload(std::optional<int> audio, std::optional<int> subtitle, jf::PlayMode mode, const std::string& banner) {
    if (switching_ || left_ || item_.media_sources.empty()) return;
    const double position =
        ui::restart_position(state_.started, state_.position_s, jf::ticks_to_seconds(plan_.start_ticks));
    begin_switch(banner);
    auto client = ctx_.client;
    const std::string id = item_.id;
    const jf::MediaSource source = item_.media_sources[0];
    async(
        [client, id, source, audio, subtitle, mode, position] {
          jf::PlayRequest req;
          req.item_id = id;
          req.source = &source;
          req.mode = mode;
          req.audio_index = audio;
          req.subtitle_index = subtitle;
          req.start_ticks = jf::seconds_to_ticks(position);
          return client->prepare_playback(req);
        },
        [this](jf::Result<jf::PlaybackPlan> r) {
          if (left_) return;
          if (!r) return fail_switch(r.error());
          plan_ = std::move(r.value());
          switching_ = false;
          start_stream();
        });
  }

  // The old stream ends here: report it, free the decoder, show the loading banner.
  void begin_switch(const std::string& banner) {
    switching_ = true;
    countdown_.cancel();
    if (reported_start_) send_report(Report::Stopped);
    reported_start_ = false;
    if (backend_) backend_->stop();
    state_ = player::PlayerState{};
    overlay_->set_banner(banner, color::kDim);
  }

  void fail_switch(const jf::Error& error) {
    report_error(ctx_, error);
    leave();
  }

  // ---------------------------------------------------------------------- tick

  void tick() {
    if (left_) return;
    const uint32_t now = platform::tick_ms();
    if (countdown_.active()) return tick_countdown(now);
    if (switching_ || !backend_) return;

    state_ = backend_->poll(now);
    if (draw_buf_ && backend_->render(static_cast<uint8_t*>(draw_buf_->data), draw_buf_->header.stride)) {
      lv_obj_invalidate(canvas_);
    }

    if (!reported_start_ && state_.started) {
      reported_start_ = true;
      scheduler_.start(now);
      send_report(Report::Start);
    } else if (reported_start_ && scheduler_.progress_due(now, state_.paused)) {
      send_report(Report::Progress);
    }

    if (state_.ended == player::EndReason::Eof) return finish_video();
    if (state_.ended == player::EndReason::Error) return on_playback_error();

    controls_.tick(now);
    update_banner(now);
    refresh_overlay(now);
  }

  // A direct play that fails is tried once more as a server transcode.
  void on_playback_error() {
    if (plan_.method == jf::Method::DirectPlay && !retried_) {
      retried_ = true;
      force_transcode_ = true;  // a later track change must not go back to the direct play that failed
      reload(index_or_none(plan_.audio_index), index_or_none(plan_.subtitle_index), jf::PlayMode::AlwaysTranscode,
             "Trying a transcode...");
      return;
    }
    ctx_.toast->show("Playback failed", Toast::Kind::Error, 3500);
    leave();
  }

  void finish_video() {
    // A dropped connection can look like an end of file. Far from the end it is a failure.
    if (!ui::reached_end(state_.position_s, state_.duration_s)) return on_playback_error();
    const double end = state_.duration_s > 0 ? state_.duration_s : state_.position_s;
    if (reported_start_) send_report(Report::Stopped, end);
    reported_start_ = false;
    const jf::Item* next = ui::adjacent_episode(episodes_, item_.id, 1);
    if (!next) return leave();
    next_id_ = next->id;
    next_label_ = ui::episode_label(*next);
    backend_->stop();  // free the decoder. The last frame stays on the canvas.
    overlay_->set_controls_visible(false);
    countdown_.start(platform::tick_ms());
    countdown_shown_ = -1;
  }

  void tick_countdown(uint32_t now) {
    if (countdown_.done(now)) {
      const std::string id = next_id_;
      countdown_.cancel();
      switch_to(id);
      return;
    }
    const int left = countdown_.seconds_left(now);
    if (left == countdown_shown_) return;
    countdown_shown_ = left;
    overlay_->set_banner("Next: " + next_label_ + " plays in " + std::to_string(left) + " s\nEsc cancels",
                         color::kText);
  }

  void update_banner(uint32_t now) {
    if (!state_.started) {
      overlay_->set_banner("Loading...", color::kDim);
    } else if (state_.buffering) {
      if (!buffering_since_) buffering_since_ = now;
      const bool slow = now - *buffering_since_ >= kNetworkSlowMs;
      overlay_->set_banner(slow ? "Network too slow" : "Buffering...", slow ? color::kWarn : color::kDim);
    } else {
      buffering_since_.reset();
      overlay_->set_banner(state_.paused && !controls_.visible() ? "Paused" : "", color::kDim);
    }
  }

  void refresh_overlay(uint32_t now) {
    const bool visible = controls_.visible() && !menu_open_;
    if (visible && (!overlay_->controls_visible() || now - status_read_ms_ >= kStatusRefreshMs)) {
      overlay_->set_status(platform::read_status(ctx_.demo));
      status_read_ms_ = now;
    }
    overlay_->set_controls_visible(visible);
    bool enabled[PlayerOverlay::kIconCount];
    for (int i = 0; i < PlayerOverlay::kIconCount; ++i) enabled[i] = controls_.icon_enabled(static_cast<ui::PlayerIcon>(i));
    overlay_->set_focus(controls_.focus(), enabled);
    overlay_->set_playing(!state_.paused);
    const double duration = state_.duration_s > 0 ? state_.duration_s : jf::ticks_to_seconds(item_.runtime_ticks);
    overlay_->set_position(state_.position_s, duration);
  }

  // --------------------------------------------------------------- track menu

  void open_track_menu() {
    if (item_.media_sources.empty()) {
      ctx_.toast->show("No tracks to choose from", Toast::Kind::Info, 2000);
      return;
    }
    const jf::MediaSource& src = item_.media_sources[0];
    std::vector<ListRow> rows;
    entries_.clear();
    size_t selected = 1;
    auto header = [&](const char* text) {
      ListRow r;
      r.header = true;
      r.primary = text;
      rows.push_back(std::move(r));
      entries_.push_back({true, false, -1});
    };
    auto choice = [&](const std::string& text, bool audio, int stream, bool current) {
      ListRow r;
      r.primary = text;
      r.watched = current;  // draws a check mark
      if (current) selected = rows.size();
      rows.push_back(std::move(r));
      entries_.push_back({false, audio, stream});
    };
    header("Audio");
    for (const jf::MediaStream* a : src.audio_streams()) choice(ui::audio_label(*a), true, a->index, a->index == plan_.audio_index);
    header("Subtitles");
    choice("Off", false, -1, plan_.subtitle_index < 0);
    for (const jf::MediaStream* s : src.subtitle_streams()) {
      choice(ui::subtitle_label(*s), false, s->index, s->index == plan_.subtitle_index);
    }

    menu_box_ = make_plain_obj(root_, 0, 0, kScreenW, kScreenH);
    lv_obj_set_style_bg_color(menu_box_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(menu_box_, LV_OPA_90, 0);
    menu_ = std::make_unique<ListView>(menu_box_, ctx_.fonts, 0, 10, kScreenW, 6);
    menu_->set_rows(std::move(rows));
    menu_->select(selected);
    menu_open_ = true;
  }

  void close_track_menu() {
    menu_.reset();
    if (menu_box_) lv_obj_delete(menu_box_);
    menu_box_ = nullptr;
    menu_open_ = false;
  }

  bool on_menu_key(const ui::KeyEvent& e) {
    if (e.key == ui::Key::Esc) {
      close_track_menu();
      return true;
    }
    if (menu_->handle_key(e)) return true;
    if (e.key == ui::Key::Enter && !e.repeat) {
      const int row = menu_->selected();
      if (row >= 0 && static_cast<size_t>(row) < entries_.size() && !entries_[static_cast<size_t>(row)].header) {
        pick(entries_[static_cast<size_t>(row)]);
      }
    }
    return true;
  }

  void pick(const MenuEntry& entry) {
    int audio = plan_.audio_index;
    int subtitle = plan_.subtitle_index;
    (entry.audio ? audio : subtitle) = entry.stream;
    close_track_menu();
    if (audio == plan_.audio_index && subtitle == plan_.subtitle_index) return;
    std::string banner = "Loading...";
    if (subtitle >= 0) {
      const jf::MediaStream* s = item_.media_sources[0].stream_by_index(subtitle);
      if (s && !s->is_text_subtitle) banner = "Burned-in subtitles start slowly";
    }
    reload(index_or_none(audio), index_or_none(subtitle),
           force_transcode_ ? jf::PlayMode::AlwaysTranscode : ctx_.play_mode(), banner);
  }

  // ------------------------------------------------------------------- reports

  jf::ProgressReport make_report(double position_s) const {
    jf::ProgressReport r;
    r.item_id = item_.id;
    r.media_source_id = plan_.media_source_id;
    r.play_session_id = plan_.play_session_id;
    r.position_ticks = jf::seconds_to_ticks(position_s);
    r.is_paused = state_.paused;
    r.method = plan_.method;
    r.audio_index = plan_.audio_index;
    r.subtitle_index = plan_.subtitle_index;
    return r;
  }

  // Reports run with a fresh token, so they still go out after this screen is gone.
  void send_report(Report kind, double position_s = -1) {
    if (!ctx_.client) return;
    const jf::ProgressReport r = make_report(position_s >= 0 ? position_s : state_.position_s);
    auto client = ctx_.client;
    ctx_.async->run(
        Async::make_alive(),
        [client, r, kind] {
          return kind == Report::Start ? client->report_start(r)
                 : kind == Report::Stopped ? client->report_stopped(r)
                                           : client->report_progress(r);
        },
        [](jf::Result<jf::Unit>) {});
  }

  void leave() {
    if (left_) return;
    left_ = true;
    if (reported_start_) send_report(Report::Stopped);
    reported_start_ = false;
    if (backend_) backend_->stop();
    ctx_.nav->pop();
  }

  jf::PlaybackPlan plan_;
  jf::Item item_;
  std::unique_ptr<player::PlayerBackend> backend_;
  std::unique_ptr<PlayerOverlay> overlay_;
  lv_obj_t* canvas_ = nullptr;
  lv_draw_buf_t* draw_buf_ = nullptr;
  ui::ControlsModel controls_;
  ui::ReportScheduler scheduler_;
  ui::Countdown countdown_;
  player::PlayerState state_;
  std::vector<jf::Item> episodes_;
  std::vector<MenuEntry> entries_;
  std::unique_ptr<ListView> menu_;
  lv_obj_t* menu_box_ = nullptr;
  std::optional<uint32_t> buffering_since_;
  std::string next_id_;
  std::string next_label_;
  int countdown_shown_ = -1;  // the seconds the banner shows, -1 = none yet
  uint32_t status_read_ms_ = 0;
  bool started_ = false;
  bool reported_start_ = false;
  bool switching_ = false;
  bool retried_ = false;
  bool force_transcode_ = false;  // set after a direct play failed: this item plays as a transcode
  bool menu_open_ = false;
  bool muted_ = false;
  bool left_ = false;
};

}  // namespace

std::unique_ptr<Screen> make_player_screen(AppContext& ctx, jf::PlaybackPlan plan, jf::Item item) {
  return std::make_unique<PlayerScreen>(ctx, std::move(plan), std::move(item));
}

}  // namespace zerofin::app
