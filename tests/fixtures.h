#pragma once

#include <string>

// JSON shaped like real Jellyfin replies, cut down to the fields the app reads.
namespace fixtures {

// One movie (H.264 Main 1080p, 5.2 Mbps, text and image subtitles) and one episode.
inline std::string items_json(long long movie_bitrate = 5'200'000, const char* video_codec = "h264",
                              const char* profile = "Main", int bit_depth = 8) {
  return std::string(R"({"Items":[{"Id":"m1","Name":"Movie One","Type":"Movie","ProductionYear":1999,)") +
         R"("RunTimeTicks":54000000000,"Overview":null,"ImageTags":{"Primary":"tagA"},)" +
         R"("UserData":{"PlaybackPositionTicks":6000000000,"Played":false,"PlayCount":2},)" +
         R"("MediaSources":[{"Id":"ms1","Container":"mkv","Bitrate":)" + std::to_string(movie_bitrate) +
         R"(,"RunTimeTicks":54000000000,"MediaStreams":[)" +
         R"({"Index":0,"Type":"Video","Codec":")" + video_codec + R"(","Profile":")" + profile +
         R"(","Width":1920,"Height":1080,"BitDepth":)" + std::to_string(bit_depth) +
         R"(,"RealFrameRate":23.976,"IsInterlaced":false},)" +
         R"({"Index":1,"Type":"Audio","Codec":"aac","Language":"jpn","Channels":2,"IsDefault":true},)" +
         R"({"Index":2,"Type":"Subtitle","Codec":"subrip","Language":"eng","IsTextSubtitleStream":true},)" +
         R"({"Index":3,"Type":"Subtitle","Codec":"PGSSUB","Language":"eng","IsTextSubtitleStream":false})" +
         R"(]}]},)" +
         R"({"Id":"e1","Name":"Pilot","Type":"Episode","SeriesId":"s1","SeriesName":"Show",)" +
         R"("SeasonId":"se1","SeasonName":"Season 1","IndexNumber":1,"ParentIndexNumber":1}],)" +
         R"("TotalRecordCount":42,"StartIndex":0})";
}

inline const char* auth_json() {
  return R"({"User":{"Id":"u1","Name":"admin"},"AccessToken":"tok","ServerId":"srv"})";
}

inline const char* playback_info_direct_json() {
  return R"({"PlaySessionId":"ps1","MediaSources":[{"Id":"ms1","Container":"mkv"}]})";
}

inline const char* playback_info_transcode_json() {
  return R"({"PlaySessionId":"ps1","MediaSources":[{"Id":"ms1","TranscodingUrl":)"
         R"("/videos/m1/master.m3u8?MediaSourceId=ms1&MaxWidth=1920&SubtitleStreamIndex=2&VideoBitrate=8000000"}]})";
}

}  // namespace fixtures
