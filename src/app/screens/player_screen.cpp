#include <algorithm>
#include <cstdlib>
#include <memory>
#include <optional>

#include "platform/platform.h"
#include "platform/status_reader.h"
#include "player/backend.h"
#include "player/output_plan.h"
#include "player/volume.h"
#include "screens/screens.h"
#include "ui/player_overlay.h"
#include "ui/widgets.h"
#include "ui_core/format.h"
#include "ui_core/menu_tabs.h"
#include "ui_core/output_watch.h"
#include "ui_core/player_controls.h"
#include "ui_core/player_timers.h"
#include "ui_core/playlist.h"

namespace zerofin::app {
namespace {

constexpr uint32_t kNetworkSlowMs = 5000;
constexpr uint32_t kStatusRefreshMs = 30000;
constexpr int kVolumeStep = 5;
constexpr uint32_t kMonitorCheckMs = 2000;
constexpr int kTabBarH = 26;  // the tab bar of the track menu

// A new item with the plan to play it.
struct Loaded {
  jf::Item item;
  jf::PlaybackPlan plan;
};

// One row of the audio and subtitle menu.
struct MenuEntry {
  enum class Kind { Audio, Subtitle, Quality, Sound, Output };
  Kind kind = Kind::Audio;
  int stream = -1;    // Jellyfin stream index. -1 = subtitles off. Audio and Subtitle rows only.
  std::string value;  // the saved value of a Quality, Sound or Output row
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
    // With a monitor the video is not on the LCD. This panel shows what plays. It sits under the controls.
    remote_ = make_plain_obj(root_, 0, 0, kScreenW, kScreenH);
    lv_obj_add_flag(remote_, LV_OBJ_FLAG_HIDDEN);
    remote_title_ = make_label(remote_, ctx_.fonts.bold, color::kText, 12, 30, kScreenW - 24, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(remote_title_, LV_TEXT_ALIGN_CENTER, 0);
    remote_info_ = make_label(remote_, ctx_.fonts.small, color::kDim, 12, 56, kScreenW - 24, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(remote_info_, LV_TEXT_ALIGN_CENTER, 0);
    overlay_ = std::make_unique<PlayerOverlay>(root_, ctx_.fonts);
    controls_.set_neighbors(false, false);
    add_timer(8, [this] { tick(); });
  }

  void on_show() override {
    if (started_) return;
    started_ = true;
    options_ = ctx_.output_plan();
    backend_ = ctx_.make_backend ? ctx_.make_backend(options_) : nullptr;
    if (!backend_) {
      ctx_.toast->show("No video player in this build", Toast::Kind::Error, 3500);
      leave();
      return;
    }
    make_volume();
    apply_layout();
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
        volume_->toggle_mute();
        backend_->set_volume(volume_->backend_percent());
        if (volume_->muted()) {
          overlay_->show_notice("Muted");
        } else {
          overlay_->show_volume(volume_->level());
        }
        break;
      case ui::PlayerAction::VolumeUp:
      case ui::PlayerAction::VolumeDown: {
        const int step = action == ui::PlayerAction::VolumeUp ? kVolumeStep : -kVolumeStep;
        volume_->change(step);  // changing the volume ends the mute
        ctx_.volume = volume_->level();
        backend_->set_volume(volume_->backend_percent());
        overlay_->show_volume(volume_->level());
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

  // ------------------------------------------------------------------- output

  bool hdmi() const { return options_.output == player::Output::Hdmi; }
  jf::Target target() const { return player::target_for(options_.output, ctx_.config.hdmi_quality); }

  // The sound uses the device volume (the CP0 speaker) or the player volume (the monitor, which has its own knob).
  void make_volume() {
    const bool monitor_sound = hdmi() && options_.audio == player::AudioSink::Hdmi;
    volume_ = std::make_unique<player::Volume>(
        !monitor_sound && ctx_.make_system_volume ? ctx_.make_system_volume() : nullptr, ctx_.volume);
  }

  // The LCD shows the video, or with a monitor a panel with the title and always visible controls.
  void apply_layout() {
    if (hdmi()) {
      lv_obj_add_flag(canvas_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(remote_, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_remove_flag(canvas_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(remote_, LV_OBJ_FLAG_HIDDEN);
    }
    controls_.set_pinned(hdmi());
    refresh_remote_text();
  }

  void refresh_remote_text() {
    if (!hdmi()) return;
    lv_label_set_text(remote_title_, ui::truncate_text(item_.name, 60).c_str());
    std::string info = std::string("On the monitor, ") + (ctx_.config.hdmi_quality == "1080" ? "1080p" : "720p");
    info += plan_.method == jf::Method::DirectPlay ? ", direct play" : ", server transcode";
    lv_label_set_text(remote_info_, info.c_str());
  }

  // Looks at the cable and the settings again. Called when a stream restarts, so the new stream goes to the
  // right place. The old backend is stopped already.
  void adopt_output() {
    player::BackendOptions fresh = fallback_to_lcd_ ? player::BackendOptions{} : ctx_.output_plan();
    const bool new_output = fresh.output != options_.output;
    options_ = fresh;
    if (new_output) {
      backend_ = ctx_.make_backend(options_);
      if (!backend_) return leave();
      make_volume();
      apply_layout();
    } else if (hdmi()) {
      backend_->set_audio_sink(options_.audio);
    }
  }

  // ------------------------------------------------------------------ starting

  void start_stream() {
    state_ = player::PlayerState{};
    reported_start_ = false;
    buffering_since_.reset();
    player::StartParams params;
    params.url = plan_.url;
    params.start_seconds = jf::ticks_to_seconds(plan_.start_ticks);
    params.subtitle_url = plan_.external_subtitle_url;
    params.volume = volume_->backend_percent();
    refresh_remote_text();
    // Hardware decode hung the player on 720p and 1080p files on the CP0. Software decode is the
    // default. ZEROFIN_HWDEC=1 turns the hardware decoder on again, for experiments.
    params.hardware_decode = plan_.method == jf::Method::DirectPlay && std::getenv("ZEROFIN_HWDEC") != nullptr;
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
    const auto target = this->target();
    async(
        [client, id, mode, target]() -> jf::Result<Loaded> {
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
          req.target = target;
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
    adopt_output();
    if (left_) return;
    auto client = ctx_.client;
    const std::string id = item_.id;
    const jf::MediaSource source = item_.media_sources[0];
    const auto target = this->target();
    async(
        [client, id, source, audio, subtitle, mode, position, target] {
          jf::PlayRequest req;
          req.item_id = id;
          req.source = &source;
          req.mode = mode;
          req.target = target;
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
    watch_monitor(now);
    update_banner(now);
    refresh_overlay(now);
  }

  // Looks at the cable every two seconds. A monitor plugged in while the video plays on the LCD gets the video.
  // A cable pulled while it plays on the monitor sends it back to the LCD. Both restart at the same position.
  void watch_monitor(uint32_t now) {
    if (switching_ || menu_open_ || now - monitor_check_ms_ < kMonitorCheckMs) return;
    monitor_check_ms_ = now;
    const bool wants_monitor = !fallback_to_lcd_ && ctx_.output_plan().output == player::Output::Hdmi;
    const ui::OutputChange change = output_watch_.check(hdmi(), wants_monitor);
    if (change == ui::OutputChange::None) return;
    const bool to_monitor = change == ui::OutputChange::ToMonitor;
    ctx_.toast->show(to_monitor ? "Monitor connected, moving the video to it" : "Monitor unplugged, playing on the LCD",
                     Toast::Kind::Info, 3500);
    reload(index_or_none(plan_.audio_index), index_or_none(plan_.subtitle_index),
           force_transcode_ ? jf::PlayMode::AlwaysTranscode : ctx_.play_mode(),
           to_monitor ? "Moving to the monitor..." : "Playing on the LCD...");
  }

  // A direct play that fails is tried once more as a server transcode.
  void on_playback_error() {
    // The monitor failed (unplugged, busy, or the GPU output could not start). Play on the LCD, once.
    if (hdmi() && !fallback_to_lcd_) {
      fallback_to_lcd_ = true;
      ctx_.toast->show("Monitor failed, playing on the LCD", Toast::Kind::Error, 3500);
      reload(index_or_none(plan_.audio_index), index_or_none(plan_.subtitle_index),
             force_transcode_ ? jf::PlayMode::AlwaysTranscode : ctx_.play_mode(), "Playing on the LCD...");
      return;
    }
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
    tabs_ = ui::menu_tabs(hdmi(), ctx_.monitor_present());
    tab_ = ui::resolve_tab(tabs_, tab_);

    menu_box_ = make_plain_obj(root_, 0, 0, kScreenW, kScreenH);
    lv_obj_set_style_bg_color(menu_box_, lv_color_black(), 0);
    // The video shows through the menu on the LCD. On a monitor the LCD has the title panel: hide it.
    lv_obj_set_style_bg_opa(menu_box_, hdmi() ? LV_OPA_COVER : LV_OPA_90, 0);

    // The tab bar: one label per tab, an underline under the current one.
    const int tab_w = kScreenW / static_cast<int>(tabs_.size());
    tab_labels_.clear();
    for (size_t i = 0; i < tabs_.size(); ++i) {
      lv_obj_t* label = make_label(menu_box_, ctx_.fonts.normal, color::kDim, static_cast<int>(i) * tab_w, 3, tab_w,
                                   LV_LABEL_LONG_MODE_CLIP);
      lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
      lv_label_set_text(label, ui::menu_tab_label(tabs_[i]));
      tab_labels_.push_back(label);
    }
    tab_underline_ = make_plain_obj(menu_box_, 0, kTabBarH - 3, tab_w, 2);
    lv_obj_set_style_bg_color(tab_underline_, lv_color_hex(color::kAccent), 0);
    lv_obj_set_style_bg_opa(tab_underline_, LV_OPA_COVER, 0);

    menu_ = std::make_unique<ListView>(menu_box_, ctx_.fonts, 0, kTabBarH, kScreenW, 5);
    menu_open_ = true;
    show_tab(tab_);
  }

  // Fills the list with the rows of one tab and marks the tab in the bar.
  void show_tab(ui::MenuTab tab) {
    tab_ = tab;
    const int tab_w = kScreenW / static_cast<int>(tabs_.size());
    for (size_t i = 0; i < tabs_.size(); ++i) {
      const bool current = tabs_[i] == tab;
      lv_obj_set_style_text_color(tab_labels_[i], lv_color_hex(current ? color::kText : color::kDim), 0);
      lv_obj_set_style_text_font(tab_labels_[i], current ? ctx_.fonts.bold : ctx_.fonts.normal, 0);
      if (current) lv_obj_set_x(tab_underline_, static_cast<int>(i) * tab_w);
    }

    const jf::MediaSource& src = item_.media_sources[0];
    std::vector<ListRow> rows;
    entries_.clear();
    size_t selected = 0;
    // `kind` says what a press of Enter does. `current` draws the check mark and starts the cursor there.
    auto row = [&](const std::string& text, MenuEntry::Kind kind, int stream, const std::string& value, bool current) {
      ListRow r;
      r.primary = text;
      r.watched = current;
      if (current) selected = rows.size();
      rows.push_back(std::move(r));
      entries_.push_back({kind, stream, value});
    };
    switch (tab) {
      case ui::MenuTab::Audio:
        for (const jf::MediaStream* a : src.audio_streams()) {
          row(ui::audio_label(*a), MenuEntry::Kind::Audio, a->index, "", a->index == plan_.audio_index);
        }
        break;
      case ui::MenuTab::Subtitles:
        row("Off", MenuEntry::Kind::Subtitle, -1, "", plan_.subtitle_index < 0);
        for (const jf::MediaStream* s : src.subtitle_streams()) {
          row(ui::subtitle_label(*s), MenuEntry::Kind::Subtitle, s->index, "", s->index == plan_.subtitle_index);
        }
        break;
      case ui::MenuTab::Quality:
        row("720p", MenuEntry::Kind::Quality, -1, "720", ctx_.config.hdmi_quality != "1080");
        row("1080p (may drop frames)", MenuEntry::Kind::Quality, -1, "1080", ctx_.config.hdmi_quality == "1080");
        break;
      case ui::MenuTab::Sound:
        row("Monitor", MenuEntry::Kind::Sound, -1, "monitor", ctx_.config.hdmi_sound != "speaker");
        row("CP0 speaker", MenuEntry::Kind::Sound, -1, "speaker", ctx_.config.hdmi_sound == "speaker");
        break;
      case ui::MenuTab::Output:
        row("Monitor", MenuEntry::Kind::Output, -1, "auto", ctx_.config.video_output != "lcd");
        row("LCD", MenuEntry::Kind::Output, -1, "lcd", ctx_.config.video_output == "lcd");
        break;
    }
    menu_->set_rows(std::move(rows));
    menu_->select(selected);
  }

  void close_track_menu() {
    menu_.reset();
    if (menu_box_) lv_obj_delete(menu_box_);
    menu_box_ = nullptr;
    tab_labels_.clear();
    tab_underline_ = nullptr;
    menu_open_ = false;
  }

  bool on_menu_key(const ui::KeyEvent& e) {
    if (e.key == ui::Key::Esc) {
      close_track_menu();
      return true;
    }
    // Left and Right (or Tab) change the tab. Up and Down move in the list.
    if ((e.key == ui::Key::Left || e.key == ui::Key::Right || e.key == ui::Key::Tab) && !e.repeat) {
      const int direction = e.key == ui::Key::Left ? -1 : 1;
      show_tab(ui::next_tab(tabs_, tab_, direction));
      return true;
    }
    if (menu_->handle_key(e)) return true;
    if (e.key == ui::Key::Enter && !e.repeat) {
      const int row = menu_->selected();
      if (row >= 0 && static_cast<size_t>(row) < entries_.size()) pick(entries_[static_cast<size_t>(row)]);
    }
    return true;
  }

  void pick(const MenuEntry& entry) {
    switch (entry.kind) {
      case MenuEntry::Kind::Quality:
        return pick_quality(entry.value);
      case MenuEntry::Kind::Sound:
        return pick_sound(entry.value);
      case MenuEntry::Kind::Output:
        return pick_output(entry.value);
      case MenuEntry::Kind::Audio:
      case MenuEntry::Kind::Subtitle:
        break;
    }
    int audio = plan_.audio_index;
    int subtitle = plan_.subtitle_index;
    (entry.kind == MenuEntry::Kind::Audio ? audio : subtitle) = entry.stream;
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

  // The server restarts the transcode at the new size. The video resumes at the same position.
  void pick_quality(const std::string& value) {
    close_track_menu();
    if (value == ctx_.config.hdmi_quality) return;
    ctx_.config.hdmi_quality = value;
    ctx_.save_config();
    reload(index_or_none(plan_.audio_index), index_or_none(plan_.subtitle_index),
           force_transcode_ ? jf::PlayMode::AlwaysTranscode : ctx_.play_mode(),
           std::string("Switching to ") + (value == "1080" ? "1080p" : "720p") + "...");
  }

  // Moves the sound at once, with no restart.
  void pick_sound(const std::string& value) {
    close_track_menu();
    if (value == ctx_.config.hdmi_sound) return;
    ctx_.config.hdmi_sound = value;
    ctx_.save_config();
    options_ = ctx_.output_plan();
    if (backend_) backend_->set_audio_sink(options_.audio);
    make_volume();
    if (backend_) backend_->set_volume(volume_->backend_percent());
    overlay_->show_notice(options_.audio == player::AudioSink::Hdmi ? "Sound: monitor" : "Sound: CP0 speaker");
  }

  // Moves the video between the monitor and the LCD. It restarts at the same position.
  void pick_output(const std::string& value) {
    close_track_menu();
    if (value == ctx_.config.video_output) return;
    ctx_.config.video_output = value;
    ctx_.save_config();
    fallback_to_lcd_ = false;  // the user chose: forget an earlier failure
    reload(index_or_none(plan_.audio_index), index_or_none(plan_.subtitle_index),
           force_transcode_ ? jf::PlayMode::AlwaysTranscode : ctx_.play_mode(),
           value == "lcd" ? "Moving to the LCD..." : "Moving to the monitor...");
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
  std::vector<ui::MenuTab> tabs_;
  ui::MenuTab tab_ = ui::MenuTab::Audio;  // the tab the menu opens on: the last one used
  std::vector<lv_obj_t*> tab_labels_;
  lv_obj_t* tab_underline_ = nullptr;
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
  std::unique_ptr<player::Volume> volume_;  // set in on_show, before the first stream
  player::BackendOptions options_;          // where the video and the sound go now
  lv_obj_t* remote_ = nullptr;              // the panel the LCD shows while the video is on a monitor
  lv_obj_t* remote_title_ = nullptr;
  lv_obj_t* remote_info_ = nullptr;
  bool fallback_to_lcd_ = false;            // the monitor failed: stay on the LCD for this video
  uint32_t monitor_check_ms_ = 0;
  ui::OutputWatch output_watch_;
  bool left_ = false;
};

}  // namespace

std::unique_ptr<Screen> make_player_screen(AppContext& ctx, jf::PlaybackPlan plan, jf::Item item) {
  return std::make_unique<PlayerScreen>(ctx, std::move(plan), std::move(item));
}

}  // namespace zerofin::app
