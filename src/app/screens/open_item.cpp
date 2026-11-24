#include "screens/screens.h"

#include "ui/widgets.h"
#include "ui_core/format.h"

namespace zerofin::app {
namespace {

std::string include_types_for(const jf::Item& folder) {
  const std::string& t = folder.collection_type;
  if (t == "movies") return "Movie";
  if (t == "tvshows") return "Series";
  if (t == "boxsets") return "BoxSet";
  if (folder.raw_type == "BoxSet") return "Movie,Series";
  if (folder.type == jf::ItemType::Library) return "Movie,Series,BoxSet";
  return "";
}

void open_season(AppContext& ctx, const jf::Item& season) {
  auto client = ctx.client;
  ItemListSpec spec;
  spec.title = season.name;
  spec.paged = false;
  spec.loader = [client, series = season.series_id, id = season.id](int, int) {
    return client->episodes(series, id);
  };
  spec.on_select = [&ctx](const jf::Item& episode) { open_item(ctx, episode); };
  spec.empty_text = "No episodes";
  ctx.nav->push(make_item_list_screen(ctx, std::move(spec)));
}

void open_series(AppContext& ctx, const jf::Item& series) {
  auto client = ctx.client;
  ItemListSpec spec;
  spec.title = series.name;
  spec.paged = false;
  spec.loader = [client, id = series.id](int, int) { return client->seasons(id); };
  spec.on_select = [&ctx](const jf::Item& season) { open_season(ctx, season); };
  spec.empty_text = "No seasons";
  ctx.nav->push(make_item_list_screen(ctx, std::move(spec)));
}

void open_folder(AppContext& ctx, const jf::Item& folder) {
  auto client = ctx.client;
  ItemListSpec spec;
  spec.title = folder.name;
  const std::string types = include_types_for(folder);
  spec.loader = [client, id = folder.id, types](int start, int limit) {
    jf::ItemQuery q;
    q.parent_id = id;
    q.include_types = types;
    q.start_index = start;
    q.limit = limit;
    return client->items(q);
  };
  spec.on_select = [&ctx](const jf::Item& item) { open_item(ctx, item); };
  ctx.nav->push(make_item_list_screen(ctx, std::move(spec)));
}

}  // namespace

void open_item(AppContext& ctx, const jf::Item& item) {
  switch (item.type) {
    case jf::ItemType::Movie:
    case jf::ItemType::Episode:
      ctx.nav->push(make_detail_screen(ctx, item));
      break;
    case jf::ItemType::Series:
      open_series(ctx, item);
      break;
    case jf::ItemType::Season:
      open_season(ctx, item);
      break;
    default:
      open_folder(ctx, item);
      break;
  }
}

void go_home(AppContext& ctx) { ctx.nav->reset(make_home_screen(ctx)); }
void go_connect(AppContext& ctx) { ctx.nav->reset(make_connect_screen(ctx)); }
void go_setup(AppContext& ctx) { ctx.nav->reset(make_setup_screen(ctx)); }

bool handle_auth_error(AppContext& ctx, const jf::Error& error) {
  if (error.kind != jf::ErrorKind::Unauthorized || !ctx.config.logged_in()) return false;
  ctx.clear_login(false);
  ctx.toast->show("Session ended. Sign in again.", Toast::Kind::Warning, 3500);
  go_connect(ctx);
  return true;
}

bool report_error(AppContext& ctx, const jf::Error& error, uint32_t toast_ms) {
  if (handle_auth_error(ctx, error)) return true;
  ctx.toast->show(error_text(error), Toast::Kind::Error, toast_ms);
  return false;
}

std::string error_text(const jf::Error& e) {
  switch (e.kind) {
    case jf::ErrorKind::Network: return "Cannot reach the server";
    case jf::ErrorKind::Unauthorized: return "Not signed in";
    case jf::ErrorKind::NotFound: return "Not found on the server";
    case jf::ErrorKind::Http: return e.http_status ? "Server error (HTTP " + std::to_string(e.http_status) + ")" : e.message;
    case jf::ErrorKind::Parse: return "The server sent an unexpected reply";
    default: return e.message.empty() ? "Something went wrong" : e.message;
  }
}

std::string reason_text(jf::Reason r) {
  switch (r) {
    case jf::Reason::Ok: return "fits this device";
    case jf::Reason::ForcedByUser: return "Always transcode is on";
    case jf::Reason::NoVideo: return "no video stream";
    case jf::Reason::VideoCodecUnsupported: return "video is not H.264";
    case jf::Reason::UnsupportedH264Profile: return "H.264 profile not supported";
    case jf::Reason::BitDepthTooHigh: return "video is 10-bit";
    case jf::Reason::ResolutionTooHigh: return "resolution above 1080p";
    case jf::Reason::BitrateTooHigh: return "bitrate above the limit";
    case jf::Reason::Interlaced: return "interlaced video";
    case jf::Reason::ImageSubtitleBurnIn: return "image subtitle needs burn-in";
  }
  return "";
}

void show_playback_plan(AppContext& ctx, const jf::PlaybackPlan& plan, const jf::Item& item) {
  MessageSpec spec;
  spec.title = "Ready to play";
  const bool direct = plan.method == jf::Method::DirectPlay;
  spec.text = std::string(direct ? "DIRECT PLAY" : "SERVER TRANSCODE") + "\nBecause: " + reason_text(plan.reason) + "\n" +
              ui::truncate_text(item.name, 34) + "\nStart " + ui::format_timecode(jf::ticks_to_seconds(plan.start_ticks)) +
              ", subtitles " +
              (plan.subtitle_index < 0 ? "off" : (plan.subtitle_burned_in ? "burned in" : "from a file")) +
              "\nThe player comes in the next build step.";
  spec.text_color = direct ? color::kGood : color::kWarn;
  ctx.nav->push(make_message_screen(ctx, std::move(spec)));
}

}  // namespace zerofin::app
