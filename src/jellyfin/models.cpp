#include "jellyfin/models.h"

#include <algorithm>
#include <nlohmann/json.hpp>

namespace zerofin::jf {
namespace {

using nlohmann::json;

// Safe getters. Jellyfin sends null for many fields, and json::value() throws on null.
std::string str(const json& j, const char* key) {
  auto it = j.find(key);
  return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

int64_t i64(const json& j, const char* key, int64_t fallback = 0) {
  auto it = j.find(key);
  return (it != j.end() && it->is_number()) ? it->get<int64_t>() : fallback;
}

double num(const json& j, const char* key) {
  auto it = j.find(key);
  return (it != j.end() && it->is_number()) ? it->get<double>() : 0.0;
}

bool flag(const json& j, const char* key) {
  auto it = j.find(key);
  return it != j.end() && it->is_boolean() && it->get<bool>();
}

StreamType stream_type(const std::string& t) {
  if (t == "Video") return StreamType::Video;
  if (t == "Audio") return StreamType::Audio;
  if (t == "Subtitle") return StreamType::Subtitle;
  return StreamType::Other;
}

ItemType item_type(const std::string& t) {
  if (t == "Movie") return ItemType::Movie;
  if (t == "Series") return ItemType::Series;
  if (t == "Season") return ItemType::Season;
  if (t == "Episode") return ItemType::Episode;
  if (t == "CollectionFolder" || t == "UserView" || t == "Folder") return ItemType::Library;
  return ItemType::Other;
}

MediaStream parse_stream(const json& j) {
  MediaStream s;
  s.index = static_cast<int>(i64(j, "Index", -1));
  s.type = stream_type(str(j, "Type"));
  s.codec = str(j, "Codec");
  s.profile = str(j, "Profile");
  s.language = str(j, "Language");
  s.title = str(j, "Title");
  s.display_title = str(j, "DisplayTitle");
  s.width = static_cast<int>(i64(j, "Width"));
  s.height = static_cast<int>(i64(j, "Height"));
  s.bit_depth = static_cast<int>(i64(j, "BitDepth"));
  s.channels = static_cast<int>(i64(j, "Channels"));
  s.bitrate = i64(j, "BitRate");
  s.fps = num(j, "RealFrameRate") > 0 ? num(j, "RealFrameRate") : num(j, "AverageFrameRate");
  s.is_default = flag(j, "IsDefault");
  s.is_forced = flag(j, "IsForced");
  s.is_interlaced = flag(j, "IsInterlaced");
  s.is_text_subtitle = flag(j, "IsTextSubtitleStream");
  s.is_external = flag(j, "IsExternal");
  return s;
}

MediaSource parse_source(const json& j) {
  MediaSource m;
  m.id = str(j, "Id");
  m.container = str(j, "Container");
  m.transcoding_url = str(j, "TranscodingUrl");
  m.bitrate = i64(j, "Bitrate");
  m.runtime_ticks = i64(j, "RunTimeTicks");
  auto it = j.find("MediaStreams");
  if (it != j.end() && it->is_array()) {
    for (const auto& s : *it) m.streams.push_back(parse_stream(s));
  }
  return m;
}

Item parse_item(const json& j) {
  Item item;
  item.id = str(j, "Id");
  item.name = str(j, "Name");
  item.raw_type = str(j, "Type");
  item.type = item_type(item.raw_type);
  item.overview = str(j, "Overview");
  item.collection_type = str(j, "CollectionType");
  item.series_id = str(j, "SeriesId");
  item.series_name = str(j, "SeriesName");
  item.season_id = str(j, "SeasonId");
  item.season_name = str(j, "SeasonName");
  item.index_number = static_cast<int>(i64(j, "IndexNumber", -1));
  item.parent_index_number = static_cast<int>(i64(j, "ParentIndexNumber", -1));
  item.production_year = static_cast<int>(i64(j, "ProductionYear"));
  item.runtime_ticks = i64(j, "RunTimeTicks");
  if (auto tags = j.find("ImageTags"); tags != j.end() && tags->is_object()) {
    item.primary_image_tag = str(*tags, "Primary");
  }
  if (auto ud = j.find("UserData"); ud != j.end() && ud->is_object()) {
    item.user_data.position_ticks = i64(*ud, "PlaybackPositionTicks");
    item.user_data.played = flag(*ud, "Played");
    item.user_data.play_count = static_cast<int>(i64(*ud, "PlayCount"));
  }
  if (auto ms = j.find("MediaSources"); ms != j.end() && ms->is_array()) {
    for (const auto& s : *ms) item.media_sources.push_back(parse_source(s));
  }
  return item;
}

template <typename T, typename F>
Result<T> guarded(std::string_view text, F&& fn) {
  try {
    json j = json::parse(text);
    if (!j.is_object()) return Error{ErrorKind::Parse, "expected a JSON object", 0};
    return fn(j);
  } catch (const json::exception& e) {
    return Error{ErrorKind::Parse, e.what(), 0};
  }
}

}  // namespace

const MediaStream* MediaSource::video() const {
  for (const auto& s : streams) {
    if (s.type == StreamType::Video) return &s;
  }
  return nullptr;
}

const MediaStream* MediaSource::stream_by_index(int index) const {
  for (const auto& s : streams) {
    if (s.index == index) return &s;
  }
  return nullptr;
}

std::vector<const MediaStream*> MediaSource::audio_streams() const {
  std::vector<const MediaStream*> out;
  for (const auto& s : streams) {
    if (s.type == StreamType::Audio) out.push_back(&s);
  }
  return out;
}

std::vector<const MediaStream*> MediaSource::subtitle_streams() const {
  std::vector<const MediaStream*> out;
  for (const auto& s : streams) {
    if (s.type == StreamType::Subtitle) out.push_back(&s);
  }
  return out;
}

const MediaStream* MediaSource::default_audio() const {
  const MediaStream* first = nullptr;
  for (const auto& s : streams) {
    if (s.type != StreamType::Audio) continue;
    if (s.is_default) return &s;
    if (!first) first = &s;
  }
  return first;
}

Result<ServerInfo> parse_server_info(std::string_view text) {
  return guarded<ServerInfo>(text, [](const json& j) -> Result<ServerInfo> {
    ServerInfo s{str(j, "Id"), str(j, "ServerName"), str(j, "Version"), str(j, "LocalAddress")};
    if (s.id.empty()) return Error{ErrorKind::Parse, "server info has no Id", 0};
    return s;
  });
}

Result<QuickConnectSession> parse_quick_connect_session(std::string_view text) {
  return guarded<QuickConnectSession>(text, [](const json& j) -> Result<QuickConnectSession> {
    QuickConnectSession q{str(j, "Secret"), str(j, "Code")};
    if (q.secret.empty() || q.code.empty()) return Error{ErrorKind::Parse, "Quick Connect reply is incomplete", 0};
    return q;
  });
}

Result<bool> parse_quick_connect_authenticated(std::string_view text) {
  return guarded<bool>(text, [](const json& j) -> Result<bool> { return flag(j, "Authenticated"); });
}

Result<AuthResult> parse_auth_result(std::string_view text) {
  return guarded<AuthResult>(text, [](const json& j) -> Result<AuthResult> {
    AuthResult a;
    a.access_token = str(j, "AccessToken");
    a.server_id = str(j, "ServerId");
    if (auto u = j.find("User"); u != j.end() && u->is_object()) {
      a.user.id = str(*u, "Id");
      a.user.name = str(*u, "Name");
    }
    if (a.access_token.empty() || a.user.id.empty()) {
      return Error{ErrorKind::Parse, "login reply has no token or user", 0};
    }
    return a;
  });
}

Result<ItemList> parse_item_list(std::string_view text) {
  return guarded<ItemList>(text, [](const json& j) -> Result<ItemList> {
    ItemList list;
    auto it = j.find("Items");
    if (it == j.end() || !it->is_array()) return Error{ErrorKind::Parse, "reply has no Items array", 0};
    for (const auto& i : *it) list.items.push_back(parse_item(i));
    list.start_index = static_cast<int>(i64(j, "StartIndex"));
    // The server sends 0 when the request turned off the total count. Never report less than we hold.
    const int held = list.start_index + static_cast<int>(list.items.size());
    list.total = std::max(static_cast<int>(i64(j, "TotalRecordCount")), held);
    return list;
  });
}

Result<PlaybackInfo> parse_playback_info(std::string_view text) {
  return guarded<PlaybackInfo>(text, [](const json& j) -> Result<PlaybackInfo> {
    PlaybackInfo p;
    if (const std::string code = str(j, "ErrorCode"); !code.empty()) {
      return Error{ErrorKind::Http, "the server cannot play this item: " + code, 0};
    }
    p.play_session_id = str(j, "PlaySessionId");
    if (auto ms = j.find("MediaSources"); ms != j.end() && ms->is_array()) {
      for (const auto& s : *ms) p.sources.push_back(parse_source(s));
    }
    if (p.sources.empty()) return Error{ErrorKind::Parse, "playback info has no media source", 0};
    return p;
  });
}

}  // namespace zerofin::jf
