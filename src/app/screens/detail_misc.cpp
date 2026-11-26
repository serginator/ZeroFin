#include <algorithm>

#include "jellyfin/resume.h"
#include "screens/screens.h"
#include "ui/widgets.h"
#include "ui_core/format.h"

namespace zerofin::app {
namespace {

// --------------------------------------------------------------------- detail

class DetailScreen : public Screen {
 public:
  DetailScreen(AppContext& ctx, jf::Item item)
      : Screen(ctx, item.type == jf::ItemType::Episode && !item.series_name.empty() ? item.series_name : "Movie"),
        item_(std::move(item)) {}

  void build() override {
    name_ = make_label(root_, ctx_.fonts.bold, color::kText, 8, 23, 304);
    meta_ = make_label(root_, ctx_.fonts.small, color::kDim, 8, 41, 304);
    lv_obj_t* box = make_plain_obj(root_, 8, 57, 304, 28);
    overview_ = make_label(box, ctx_.fonts.small, color::kText, 0, 0, 304, LV_LABEL_LONG_MODE_WRAP);
    options_ = std::make_unique<ListView>(root_, ctx_.fonts, 0, 87, kScreenW, 3);
    options_->set_detail_width(40);
    refresh_texts();
    options_->set_message("Loading...");
  }

  void on_show() override {
    // Shown again after the player closed: load the item again, so "Resume from" is current.
    if (fetched_ && !refresh_on_return_) return;
    refresh_on_return_ = false;
    fetched_ = true;
    auto client = ctx_.client;
    auto id = item_.id;
    async([client, id] { return client->item(id); }, [this](jf::Result<jf::Item> r) {
      if (!r) {
        if (handle_auth_error(ctx_, r.error())) return;
        options_->set_message(error_text(r.error()), color::kError);
        return;
      }
      item_ = std::move(r.value());
      loaded_ = true;
      prepare_tracks();
      refresh_texts();
      rebuild();
    });
  }

  bool on_key(const ui::KeyEvent& e) override {
    if (busy_) return true;
    if (options_->handle_key(e)) return true;
    const int s = options_->selected();
    if (s < 0 || static_cast<size_t>(s) >= opts_.size()) return false;
    const Opt opt = opts_[static_cast<size_t>(s)];

    if (e.key == ui::Key::Left || e.key == ui::Key::Right) {
      const int dir = e.key == ui::Key::Right ? 1 : -1;
      if (opt == Opt::Audio) cycle(audio_sel_, audio_ids_.size(), dir);
      if (opt == Opt::Subtitles) cycle(sub_sel_, sub_ids_.size(), dir);
      if (opt == Opt::Audio || opt == Opt::Subtitles) {
        rebuild();
        return true;
      }
    }
    if (e.key == ui::Key::Enter && !e.repeat) {
      switch (opt) {
        case Opt::Play: start(resume_ticks_ > 0); break;
        case Opt::Restart: start(false); break;
        case Opt::Audio: cycle(audio_sel_, audio_ids_.size(), 1); rebuild(); break;
        case Opt::Subtitles: cycle(sub_sel_, sub_ids_.size(), 1); rebuild(); break;
      }
      return true;
    }
    return false;
  }

 private:
  enum class Opt { Play, Restart, Audio, Subtitles };

  static void cycle(size_t& index, size_t count, int dir) {
    if (count < 2) return;
    index = (index + count + (dir > 0 ? 1 : count - 1)) % count;
  }

  const jf::MediaSource* source() const { return item_.media_sources.empty() ? nullptr : &item_.media_sources[0]; }

  void prepare_tracks() {
    audio_ids_.clear();
    sub_ids_ = {-1};
    audio_sel_ = sub_sel_ = 0;
    const jf::MediaSource* src = source();
    if (!src) return;
    const jf::MediaStream* def = src->default_audio();
    for (const auto* a : src->audio_streams()) {
      if (def && a->index == def->index) audio_sel_ = audio_ids_.size();
      audio_ids_.push_back(a->index);
    }
    for (const auto* s : src->subtitle_streams()) sub_ids_.push_back(s->index);
    resume_ticks_ = jf::resume_start_ticks(item_.user_data, item_.runtime_ticks);
  }

  void refresh_texts() {
    lv_label_set_text(name_, item_.name.c_str());
    std::string meta;
    auto add = [&](const std::string& part) {
      if (part.empty()) return;
      meta += meta.empty() ? part : "  ·  " + part;
    };
    add(ui::episode_label(item_));
    add(item_.production_year > 0 ? std::to_string(item_.production_year) : "");
    add(ui::format_runtime(item_.runtime_ticks));
    if (const jf::MediaSource* src = source()) {
      if (const jf::MediaStream* v = src->video()) {
        std::string tech = v->height > 0 ? std::to_string(v->height) + "p" : "";
        if (!v->codec.empty()) {
          std::string c = v->codec;
          std::transform(c.begin(), c.end(), c.begin(), [](unsigned char ch) { return std::toupper(ch); });
          tech += tech.empty() ? c : " " + c;
        }
        add(tech);
      }
      add(ui::format_bitrate(src->bitrate));
    }
    lv_label_set_text(meta_, meta.c_str());
    lv_label_set_text(overview_, item_.overview.c_str());
  }

  void rebuild() {
    if (!loaded_) return;
    if (!source()) {
      options_->set_message("No playable file on the server", color::kError);
      return;
    }
    std::vector<ListRow> rows;
    opts_.clear();
    auto add = [&](Opt opt, ListRow row) {
      rows.push_back(std::move(row));
      opts_.push_back(opt);
    };

    ListRow play;
    if (resume_ticks_ > 0) {
      play.primary = "Resume from " + ui::format_timecode(jf::ticks_to_seconds(resume_ticks_));
      play.progress = jf::progress_fraction(item_.user_data.position_ticks, item_.runtime_ticks);
    } else {
      play.primary = item_.user_data.played ? "Play again" : "Play";
    }
    play.detail = "Enter";
    add(Opt::Play, std::move(play));
    if (resume_ticks_ > 0) {
      ListRow r;
      r.primary = "Start from the beginning";
      add(Opt::Restart, std::move(r));
    }

    const jf::MediaSource* src = source();
    ListRow audio;
    if (audio_ids_.empty()) {
      audio.primary = "Audio: none";
      audio.dim = true;
    } else {
      const jf::MediaStream* a = src->stream_by_index(audio_ids_[audio_sel_]);
      audio.primary = "Audio: " + (a ? ui::audio_label(*a) : std::string("?"));
      audio.detail = audio_ids_.size() > 1 ? "\xE2\x97\x80 \xE2\x96\xB6" : "";
    }
    add(Opt::Audio, std::move(audio));

    ListRow subs;
    if (sub_ids_[sub_sel_] < 0) {
      subs.primary = "Subtitles: off";
    } else {
      const jf::MediaStream* s = src->stream_by_index(sub_ids_[sub_sel_]);
      subs.primary = "Subtitles: " + (s ? ui::subtitle_label(*s) : std::string("?"));
    }
    subs.detail = sub_ids_.size() > 1 ? "\xE2\x97\x80 \xE2\x96\xB6" : "";
    add(Opt::Subtitles, std::move(subs));

    options_->set_rows(std::move(rows));
  }

  void start(bool resume) {
    if (busy_ || !source()) return;
    busy_ = true;
    set_status("Preparing...", color::kAccent);
    const std::string id = item_.id;
    const jf::MediaSource source = item_.media_sources[0];
    auto client = ctx_.client;
    const auto mode = ctx_.play_mode();
    const auto target = ctx_.playback_target();
    std::optional<int> audio;
    if (!audio_ids_.empty()) audio = audio_ids_[audio_sel_];
    std::optional<int> sub;
    if (sub_ids_[sub_sel_] >= 0) sub = sub_ids_[sub_sel_];
    const int64_t start_ticks = resume ? resume_ticks_ : 0;
    async(
        [id, source, client, mode, target, audio, sub, start_ticks] {
          jf::PlayRequest req;
          req.item_id = id;
          req.source = &source;
          req.mode = mode;
          req.target = target;
          req.audio_index = audio;
          req.subtitle_index = sub;
          req.start_ticks = start_ticks;
          return client->prepare_playback(req);
        },
        [this](jf::Result<jf::PlaybackPlan> r) {
          busy_ = false;
          set_status("");
          if (!r) {
            report_error(ctx_, r.error());
            return;
          }
          refresh_on_return_ = true;
          if (ctx_.start_playback) ctx_.start_playback(r.value(), item_);
        });
  }

  jf::Item item_;
  lv_obj_t* name_ = nullptr;
  lv_obj_t* meta_ = nullptr;
  lv_obj_t* overview_ = nullptr;
  std::unique_ptr<ListView> options_;
  std::vector<Opt> opts_;
  std::vector<int> audio_ids_;
  std::vector<int> sub_ids_;
  size_t audio_sel_ = 0;
  size_t sub_sel_ = 0;
  int64_t resume_ticks_ = 0;
  bool fetched_ = false;
  bool loaded_ = false;
  bool busy_ = false;
  bool refresh_on_return_ = false;
};

// -------------------------------------------------------------------- settings

class SettingsScreen : public Screen {
 public:
  explicit SettingsScreen(AppContext& ctx) : Screen(ctx, "Settings") {}

  void build() override {
    list_ = std::make_unique<ListView>(root_, ctx_.fonts, 0, kTitleH, kScreenW, 6);
    rebuild();
  }

  bool on_key(const ui::KeyEvent& e) override {
    if (list_->handle_key(e)) return true;
    const bool toggle = e.key == ui::Key::Enter || e.key == ui::Key::Left || e.key == ui::Key::Right;
    if (!toggle || e.repeat) return false;
    const int s = list_->selected();
    if (s >= 0 && static_cast<size_t>(s) < actions_.size() && actions_[static_cast<size_t>(s)]) {
      actions_[static_cast<size_t>(s)]();
      return true;
    }
    return false;
  }

 private:
  void rebuild() {
    std::vector<ListRow> rows;
    actions_.clear();
    auto header = [&](const std::string& text) {
      ListRow r;
      r.header = true;
      r.primary = text;
      rows.push_back(std::move(r));
      actions_.emplace_back();
    };
    auto item = [&](const std::string& primary, const std::string& detail, bool dim, std::function<void()> action) {
      ListRow r;
      r.primary = primary;
      r.detail = detail;
      r.dim = dim;
      rows.push_back(std::move(r));
      actions_.push_back(std::move(action));
    };

    header("Playback");
    item("Always transcode", ctx_.config.play_mode == "transcode" ? "On" : "Off", false, [this] {
      ctx_.config.play_mode = ctx_.config.play_mode == "transcode" ? "auto" : "transcode";
      ctx_.save_config();
      rebuild();
    });
    header("Monitor (HDMI)");
    item("Video output", ctx_.config.video_output == "lcd" ? "LCD only" : "Auto", false, [this] {
      ctx_.config.video_output = ctx_.config.video_output == "lcd" ? "auto" : "lcd";
      ctx_.save_config();
      rebuild();
    });
    item("Quality", ctx_.config.hdmi_quality == "1080" ? "1080p" : "720p", false, [this] {
      ctx_.config.hdmi_quality = ctx_.config.hdmi_quality == "1080" ? "720" : "1080";
      ctx_.save_config();
      rebuild();
    });
    item("Sound", ctx_.config.hdmi_sound == "speaker" ? "CP0 speaker" : "Monitor", false, [this] {
      ctx_.config.hdmi_sound = ctx_.config.hdmi_sound == "speaker" ? "monitor" : "speaker";
      ctx_.save_config();
      rebuild();
    });
    header("Account");
    item("Server", ctx_.config.server_name, true, nullptr);
    item("User", ctx_.config.user_name, true, nullptr);
    item("Log out", "", false, [this] { confirm_logout(); });
    item("Switch server", "", false, [this] {
      ctx_.clear_login(true);
      go_setup(ctx_);
    });
    header("About");
    item("ZeroFin", ZEROFIN_VERSION_STRING, true, nullptr);
    list_->set_rows(std::move(rows));
  }

  void confirm_logout() {
    MessageSpec spec;
    spec.title = "Log out";
    spec.text = "Log out of " + ctx_.config.server_name + "?";
    spec.confirm_label = "log out";
    spec.on_confirm = [this] {
      // Tell the server first so the token stops working, then forget it here.
      auto old_client = ctx_.client;
      ctx_.async->run(Async::make_alive(), [old_client] { return old_client->logout(); }, [](jf::Result<jf::Unit>) {});
      ctx_.clear_login(false);
      go_connect(ctx_);
    };
    ctx_.nav->push(make_message_screen(ctx_, std::move(spec)));
  }

  std::unique_ptr<ListView> list_;
  std::vector<std::function<void()>> actions_;
};

// --------------------------------------------------------------------- message

class MessageScreen : public Screen {
 public:
  MessageScreen(AppContext& ctx, MessageSpec spec) : Screen(ctx, spec.title), spec_(std::move(spec)) {}

  void build() override {
    lv_obj_t* text = make_label(root_, ctx_.fonts.normal, spec_.text_color, 10, 30, 300, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(text, spec_.text.c_str());
    lv_obj_t* hint = make_label(root_, ctx_.fonts.small, color::kDim, 10, 148, 300);
    lv_label_set_text(hint, spec_.confirm_label.empty() ? "Esc: back"
                                                         : ("Enter: " + spec_.confirm_label + "   Esc: cancel").c_str());
  }

  bool on_key(const ui::KeyEvent& e) override {
    if (e.key == ui::Key::Enter && !e.repeat && !spec_.confirm_label.empty()) {
      if (spec_.on_confirm) spec_.on_confirm();
      return true;
    }
    return false;
  }

 private:
  MessageSpec spec_;
};

}  // namespace

std::unique_ptr<Screen> make_detail_screen(AppContext& ctx, jf::Item item) {
  return std::make_unique<DetailScreen>(ctx, std::move(item));
}
std::unique_ptr<Screen> make_settings_screen(AppContext& ctx) { return std::make_unique<SettingsScreen>(ctx); }
std::unique_ptr<Screen> make_message_screen(AppContext& ctx, MessageSpec spec) {
  return std::make_unique<MessageScreen>(ctx, std::move(spec));
}

}  // namespace zerofin::app
