#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "jellyfin/result.h"

namespace zerofin::jf {

// Jellyfin counts time in ticks of 100 ns.
constexpr int64_t kTicksPerSecond = 10'000'000;
constexpr int64_t seconds_to_ticks(double seconds) { return static_cast<int64_t>(seconds * kTicksPerSecond); }
constexpr double ticks_to_seconds(int64_t ticks) { return static_cast<double>(ticks) / kTicksPerSecond; }

struct ServerInfo {
  std::string id;
  std::string name;
  std::string version;
};

struct QuickConnectSession {
  std::string secret;
  std::string code;
};

struct User {
  std::string id;
  std::string name;
};

struct AuthResult {
  std::string access_token;
  std::string server_id;
  User user;
};

enum class StreamType { Video, Audio, Subtitle, Other };

struct MediaStream {
  int index = -1;
  StreamType type = StreamType::Other;
  std::string codec;
  std::string profile;
  std::string language;
  std::string title;
  int width = 0;
  int height = 0;
  int bit_depth = 0;
  int channels = 0;
  int64_t bitrate = 0;
  double fps = 0;
  bool is_default = false;
  bool is_forced = false;
  bool is_interlaced = false;
  bool is_text_subtitle = false;
};

struct MediaSource {
  std::string id;
  std::string container;
  std::string transcoding_url;  // relative to the server, empty when none
  int64_t bitrate = 0;
  int64_t runtime_ticks = 0;
  std::vector<MediaStream> streams;

  const MediaStream* video() const;
  const MediaStream* stream_by_index(int index) const;
  std::vector<const MediaStream*> audio_streams() const;
  std::vector<const MediaStream*> subtitle_streams() const;
  // Default audio stream, or the first one, or null.
  const MediaStream* default_audio() const;
};

struct UserData {
  int64_t position_ticks = 0;
  bool played = false;
  int play_count = 0;
};

enum class ItemType { Movie, Series, Season, Episode, Library, Other };

struct Item {
  std::string id;
  std::string name;
  std::string raw_type;
  ItemType type = ItemType::Other;
  std::string overview;
  std::string collection_type;  // for libraries: "movies", "tvshows", ...
  std::string series_id;
  std::string series_name;
  std::string season_id;
  std::string primary_image_tag;
  int index_number = -1;         // episode number, or season number for seasons
  int parent_index_number = -1;  // season number for episodes
  int production_year = 0;
  int64_t runtime_ticks = 0;
  UserData user_data;
  std::vector<MediaSource> media_sources;
};

struct ItemList {
  std::vector<Item> items;
  int total = 0;       // TotalRecordCount, or items.size() when the server omits it
  int start_index = 0;
};

struct PlaybackInfo {
  std::string play_session_id;
  std::vector<MediaSource> sources;
};

Result<ServerInfo> parse_server_info(std::string_view json);
Result<QuickConnectSession> parse_quick_connect_session(std::string_view json);
Result<bool> parse_quick_connect_authenticated(std::string_view json);
Result<AuthResult> parse_auth_result(std::string_view json);
Result<ItemList> parse_item_list(std::string_view json);
Result<PlaybackInfo> parse_playback_info(std::string_view json);

}  // namespace zerofin::jf
