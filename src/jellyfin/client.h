#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "jellyfin/auth.h"
#include "jellyfin/device_profile.h"
#include "jellyfin/http.h"
#include "jellyfin/models.h"
#include "jellyfin/playback_decision.h"
#include "jellyfin/result.h"
#include "jellyfin/url.h"

namespace zerofin::jf {

struct Session {
  std::string token;
  std::string user_id;
  bool logged_in() const { return !token.empty() && !user_id.empty(); }
};

struct ItemQuery {
  std::string parent_id;
  std::string include_types;  // e.g. "Movie,Series"
  int start_index = 0;
  int limit = 50;
};

struct PlayRequest {
  std::string item_id;
  const MediaSource* source = nullptr;  // chosen media source from the item
  PlayMode mode = PlayMode::Auto;
  std::optional<int> audio_index;       // Jellyfin stream index, nullopt = default
  std::optional<int> subtitle_index;    // nullopt = subtitles off
  int64_t start_ticks = 0;
};

// What the player needs to start a stream.
struct PlaybackPlan {
  Method method = Method::DirectPlay;
  Reason reason = Reason::Ok;
  std::string url;                       // contains the access token. Use redact_url() to log it.
  std::string play_session_id;
  std::string media_source_id;
  int audio_index = -1;
  int subtitle_index = -1;               // -1 = off
  bool subtitle_burned_in = false;
  std::optional<std::string> external_subtitle_url;  // text subtitle file, drawn by the player
  int64_t start_ticks = 0;
};

struct ProgressReport {
  std::string item_id;
  std::string media_source_id;
  std::string play_session_id;
  int64_t position_ticks = 0;
  bool is_paused = false;
  Method method = Method::DirectPlay;
  int audio_index = -1;
  int subtitle_index = -1;
};

class Client {
 public:
  Client(HttpClient& http, ClientInfo info, std::string base_url);

  void set_session(Session session) { session_ = std::move(session); }
  const Session& session() const { return session_; }
  void set_profile(PlaybackProfile profile) { profile_ = profile; }

  // Server and login.
  Result<ServerInfo> public_info();
  Result<QuickConnectSession> quick_connect_initiate();
  Result<bool> quick_connect_authorized(const std::string& secret);
  Result<AuthResult> login_quick_connect(const std::string& secret);  // also sets the session
  Result<AuthResult> login_password(const std::string& user, const std::string& password);
  Result<Unit> logout();

  // Browse.
  Result<ItemList> views();
  Result<ItemList> resume_items(int limit = 12);
  Result<ItemList> next_up(int limit = 12);
  // The newest items of a library. Episodes come grouped as their series.
  Result<ItemList> latest(const std::string& parent_id, int limit = 16);
  Result<ItemList> items(const ItemQuery& query);
  Result<ItemList> seasons(const std::string& series_id);
  Result<ItemList> episodes(const std::string& series_id, const std::string& season_id);
  Result<ItemList> search(const std::string& term, int limit = 30);
  Result<Item> item(const std::string& id);  // includes media sources

  // Playback.
  Result<PlaybackPlan> prepare_playback(const PlayRequest& request);
  Result<Unit> report_start(const ProgressReport& report);
  Result<Unit> report_progress(const ProgressReport& report);
  Result<Unit> report_stopped(const ProgressReport& report);

  // URLs that need no request.
  // A poster request. It is built here and sent by jf::fetch_image on another thread, so it holds the URL
  // and the Authorization header. It does not use the client again.
  HttpRequest image_request(const ImageRef& ref, int max_width = 64, int max_height = 96) const;
  std::string subtitle_url(const std::string& item_id, const std::string& source_id, int stream_index,
                           const std::string& format = "srt") const;

 private:
  Result<std::string> request(const std::string& method, const std::string& path, const QueryParams& query = {},
                              const std::string& body = {});
  Result<AuthResult> login(const std::string& path, const std::string& body);  // also sets the session
  Result<ItemList> list_request(const std::string& path, const QueryParams& query);
  Result<Unit> report(const char* path, const ProgressReport& report, bool with_pause);
  std::string url_for(const std::string& path, const QueryParams& query = {}) const;

  HttpClient& http_;
  ClientInfo info_;
  std::string base_url_;
  Session session_;
  PlaybackProfile profile_;
};

}  // namespace zerofin::jf
