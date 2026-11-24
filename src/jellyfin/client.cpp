#include "jellyfin/client.h"

#include <cctype>
#include <nlohmann/json.hpp>

#include "jellyfin/url.h"

namespace zerofin::jf {
namespace {

using nlohmann::json;

Error not_logged_in() { return Error{ErrorKind::Unauthorized, "not logged in", 0}; }

template <typename T, typename Parse>
Result<T> parse_reply(const Result<std::string>& reply, Parse parse) {
  if (!reply) return reply.error();
  return parse(reply.value());
}

Result<Unit> to_unit(const Result<std::string>& reply) {
  if (!reply) return reply.error();
  return Unit{};
}

const char* play_method_name(Method m) { return m == Method::DirectPlay ? "DirectPlay" : "Transcode"; }

std::string lower_ascii(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

}  // namespace

Client::Client(HttpClient& http, ClientInfo info, std::string base_url)
    : http_(http), info_(std::move(info)), base_url_(std::move(base_url)) {}

std::string Client::url_for(const std::string& path, const QueryParams& query) const {
  return with_query(join_url(base_url_, path), query);
}

Result<std::string> Client::request(const std::string& method, const std::string& path, const QueryParams& query,
                                    const std::string& body) {
  HttpRequest req;
  req.method = method;
  req.url = url_for(path, query);
  req.headers.push_back({"Authorization", authorization_header_value(info_, session_.token)});
  req.headers.push_back({"Accept", "application/json"});
  if (!body.empty()) {
    req.headers.push_back({"Content-Type", "application/json"});
    req.body = body;
  }
  const HttpResponse res = http_.send(req);
  if (res.status == 0) return Error{ErrorKind::Network, res.error.empty() ? "no response" : res.error, 0};
  if (res.status == 401) return Error{ErrorKind::Unauthorized, "the server rejected the login", 401};
  if (res.status == 404) return Error{ErrorKind::NotFound, "not found", 404};
  if (res.status >= 400) return Error{ErrorKind::Http, "HTTP " + std::to_string(res.status), res.status};
  return res.body;
}

// ---- server and login ----

Result<ServerInfo> Client::public_info() {
  return parse_reply<ServerInfo>(request("GET", "/System/Info/Public"), parse_server_info);
}

Result<QuickConnectSession> Client::quick_connect_initiate() {
  return parse_reply<QuickConnectSession>(request("POST", "/QuickConnect/Initiate"), parse_quick_connect_session);
}

Result<bool> Client::quick_connect_authorized(const std::string& secret) {
  return parse_reply<bool>(request("GET", "/QuickConnect/Connect", {{"secret", secret}}),
                           parse_quick_connect_authenticated);
}

Result<AuthResult> Client::login_quick_connect(const std::string& secret) {
  auto result = parse_reply<AuthResult>(
      request("POST", "/Users/AuthenticateWithQuickConnect", {}, json{{"Secret", secret}}.dump()),
      parse_auth_result);
  if (result) session_ = Session{result.value().access_token, result.value().user.id};
  return result;
}

Result<AuthResult> Client::login_password(const std::string& user, const std::string& password) {
  auto result = parse_reply<AuthResult>(
      request("POST", "/Users/AuthenticateByName", {}, json{{"Username", user}, {"Pw", password}}.dump()),
      parse_auth_result);
  if (result) session_ = Session{result.value().access_token, result.value().user.id};
  return result;
}

Result<Unit> Client::logout() {
  if (!session_.logged_in()) return not_logged_in();
  auto reply = to_unit(request("POST", "/Sessions/Logout"));
  if (reply) session_ = Session{};
  return reply;
}

// ---- browse ----

Result<ItemList> Client::views() {
  if (!session_.logged_in()) return not_logged_in();
  return parse_reply<ItemList>(request("GET", "/UserViews", {{"userId", session_.user_id}}), parse_item_list);
}

Result<ItemList> Client::resume_items(int limit) {
  if (!session_.logged_in()) return not_logged_in();
  return parse_reply<ItemList>(request("GET", "/UserItems/Resume",
                                       {{"userId", session_.user_id},
                                        {"limit", std::to_string(limit)},
                                        {"mediaTypes", "Video"},
                                        {"fields", "Overview"},
                                        {"enableTotalRecordCount", "false"}}),
                               parse_item_list);
}

Result<ItemList> Client::next_up(int limit) {
  if (!session_.logged_in()) return not_logged_in();
  return parse_reply<ItemList>(request("GET", "/Shows/NextUp",
                                       {{"userId", session_.user_id},
                                        {"limit", std::to_string(limit)},
                                        {"fields", "Overview"},
                                        {"enableTotalRecordCount", "false"}}),
                               parse_item_list);
}

Result<ItemList> Client::items(const ItemQuery& q) {
  if (!session_.logged_in()) return not_logged_in();
  QueryParams params = {{"userId", session_.user_id},
                        {"sortBy", q.sort_by},
                        {"sortOrder", q.sort_order},
                        {"startIndex", std::to_string(q.start_index)},
                        {"limit", std::to_string(q.limit)},
                        {"fields", "Overview"}};
  if (!q.parent_id.empty()) params.push_back({"parentId", q.parent_id});
  if (!q.include_types.empty()) {
    params.push_back({"includeItemTypes", q.include_types});
    params.push_back({"recursive", "true"});
  }
  return parse_reply<ItemList>(request("GET", "/Items", params), parse_item_list);
}

Result<ItemList> Client::seasons(const std::string& series_id) {
  if (!session_.logged_in()) return not_logged_in();
  return parse_reply<ItemList>(
      request("GET", "/Shows/" + percent_encode(series_id) + "/Seasons", {{"userId", session_.user_id}}),
      parse_item_list);
}

Result<ItemList> Client::episodes(const std::string& series_id, const std::string& season_id) {
  if (!session_.logged_in()) return not_logged_in();
  return parse_reply<ItemList>(request("GET", "/Shows/" + percent_encode(series_id) + "/Episodes",
                                       {{"userId", session_.user_id}, {"seasonId", season_id}, {"fields", "Overview"}}),
                               parse_item_list);
}

Result<ItemList> Client::search(const std::string& term, int limit) {
  if (!session_.logged_in()) return not_logged_in();
  return parse_reply<ItemList>(request("GET", "/Items",
                                       {{"userId", session_.user_id},
                                        {"searchTerm", term},
                                        {"includeItemTypes", "Movie,Series"},
                                        {"recursive", "true"},
                                        {"limit", std::to_string(limit)},
                                        {"fields", "Overview"}}),
                               parse_item_list);
}

Result<Item> Client::item(const std::string& id) {
  if (!session_.logged_in()) return not_logged_in();
  auto list = parse_reply<ItemList>(request("GET", "/Items",
                                            {{"userId", session_.user_id},
                                             {"ids", id},
                                             {"fields", "MediaSources,Overview"}}),
                                    parse_item_list);
  if (!list) return list.error();
  if (list.value().items.empty()) return Error{ErrorKind::NotFound, "item not found", 404};
  return list.value().items.front();
}

// ---- playback ----

Result<PlaybackPlan> Client::prepare_playback(const PlayRequest& req) {
  if (!session_.logged_in()) return not_logged_in();
  if (!req.source || req.item_id.empty()) return Error{ErrorKind::Invalid, "no item or media source", 0};
  const MediaSource& source = *req.source;

  const Decision decision = decide_playback(source, profile_, req.mode, req.subtitle_index);
  const bool direct = decision.method == Method::DirectPlay;
  const PlayMode profile_mode = direct ? PlayMode::Auto : PlayMode::AlwaysTranscode;

  const MediaStream* audio = req.audio_index ? source.stream_by_index(*req.audio_index) : source.default_audio();
  const int audio_index = audio ? audio->index : -1;
  const int subtitle_index = req.subtitle_index.value_or(-1);

  // Subtitles off must be sent as -1, or the server burns in the user's default subtitle.
  const int sent_subtitle = (req.subtitle_index && decision.burn_in_subtitle) ? subtitle_index : -1;

  QueryParams query = {{"userId", session_.user_id},
                       {"mediaSourceId", source.id},
                       {"startTimeTicks", std::to_string(req.start_ticks)},
                       {"subtitleStreamIndex", std::to_string(sent_subtitle)},
                       {"enableDirectPlay", direct ? "true" : "false"},
                       {"enableDirectStream", direct ? "true" : "false"},
                       {"enableTranscoding", direct ? "false" : "true"},
                       {"autoOpenLiveStream", "false"},
                       {"maxStreamingBitrate",
                        std::to_string(direct ? profile_.direct_max_bitrate
                                              : profile_.transcode_video_bitrate + profile_.transcode_audio_bitrate)}};
  if (audio_index >= 0) query.push_back({"audioStreamIndex", std::to_string(audio_index)});

  const json body = {{"DeviceProfile", json::parse(build_device_profile_json(profile_, profile_mode))}};
  auto info = parse_reply<PlaybackInfo>(
      request("POST", "/Items/" + percent_encode(req.item_id) + "/PlaybackInfo", query, body.dump()),
      parse_playback_info);
  if (!info) return info.error();

  const MediaSource* chosen = &info.value().sources.front();
  for (const auto& s : info.value().sources) {
    if (s.id == source.id) chosen = &s;
  }

  PlaybackPlan plan;
  plan.method = decision.method;
  plan.reason = decision.reason;
  plan.play_session_id = info.value().play_session_id;
  plan.media_source_id = chosen->id.empty() ? source.id : chosen->id;
  plan.audio_index = audio_index;
  plan.subtitle_index = subtitle_index;
  plan.subtitle_burned_in = decision.burn_in_subtitle;
  plan.start_ticks = req.start_ticks;

  if (direct) {
    plan.url = url_for("/Videos/" + percent_encode(req.item_id) + "/stream",
                       {{"static", "true"},
                        {"MediaSourceId", plan.media_source_id},
                        {"PlaySessionId", plan.play_session_id},
                        {"api_key", session_.token}});
  } else {
    if (chosen->transcoding_url.empty()) {
      return Error{ErrorKind::Http, "the server gave no transcoding URL", 0};
    }
    std::string url = join_url(base_url_, chosen->transcoding_url);
    url = set_query_param(url, "maxWidth", std::to_string(profile_.transcode_max_width));
    url = set_query_param(url, "maxHeight", std::to_string(profile_.transcode_max_height));
    url = set_query_param(url, "MaxFramerate", std::to_string(profile_.transcode_max_fps));
    url = set_query_param(url, "VideoBitrate", std::to_string(profile_.transcode_video_bitrate));
    url = set_query_param(url, "AudioBitrate", std::to_string(profile_.transcode_audio_bitrate));
    url = set_query_param(url, "SubtitleStreamIndex", std::to_string(sent_subtitle));
    if (decision.burn_in_subtitle) url = set_query_param(url, "SubtitleMethod", "Encode");
    if (audio_index >= 0) url = set_query_param(url, "AudioStreamIndex", std::to_string(audio_index));
    url = set_query_param(url, "PlaySessionId", plan.play_session_id);
    // The server already puts "ApiKey" in its transcoding URL. Add ours only when it is missing.
    const bool has_key = get_query_param(url, "ApiKey").has_value() || get_query_param(url, "api_key").has_value();
    plan.url = has_key ? url : set_query_param(url, "api_key", session_.token);
  }

  if (req.subtitle_index && !decision.burn_in_subtitle) {
    const MediaStream* sub = source.stream_by_index(subtitle_index);
    const std::string codec = sub ? lower_ascii(sub->codec) : "";
    const std::string format = (codec == "ass" || codec == "ssa") ? "ass" : "srt";
    plan.external_subtitle_url = subtitle_url(req.item_id, plan.media_source_id, subtitle_index, format);
  }
  return plan;
}

Result<Unit> Client::report(const char* path, const ProgressReport& r, bool with_pause) {
  if (!session_.logged_in()) return not_logged_in();
  json body = {{"ItemId", r.item_id},
               {"MediaSourceId", r.media_source_id},
               {"PlaySessionId", r.play_session_id},
               {"PositionTicks", r.position_ticks},
               {"PlayMethod", play_method_name(r.method)},
               {"SubtitleStreamIndex", r.subtitle_index},
               {"CanSeek", true}};
  if (r.audio_index >= 0) body["AudioStreamIndex"] = r.audio_index;
  if (with_pause) body["IsPaused"] = r.is_paused;
  return to_unit(request("POST", path, {}, body.dump()));
}

Result<Unit> Client::report_start(const ProgressReport& r) { return report("/Sessions/Playing", r, false); }
Result<Unit> Client::report_progress(const ProgressReport& r) { return report("/Sessions/Playing/Progress", r, true); }
Result<Unit> Client::report_stopped(const ProgressReport& r) { return report("/Sessions/Playing/Stopped", r, false); }

// ---- URLs ----

std::string Client::image_url(const std::string& item_id, const std::string& tag, int max_width,
                              int max_height) const {
  QueryParams q = {{"maxWidth", std::to_string(max_width)}, {"maxHeight", std::to_string(max_height)},
                   {"quality", "80"}};
  if (!tag.empty()) q.push_back({"tag", tag});
  return url_for("/Items/" + percent_encode(item_id) + "/Images/Primary", q);
}

std::string Client::subtitle_url(const std::string& item_id, const std::string& source_id, int stream_index,
                                 const std::string& format) const {
  return url_for("/Videos/" + percent_encode(item_id) + "/" + percent_encode(source_id) + "/Subtitles/" +
                     std::to_string(stream_index) + "/0/Stream." + percent_encode(format),
                 {{"api_key", session_.token}});
}

}  // namespace zerofin::jf
