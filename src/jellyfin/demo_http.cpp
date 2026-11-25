#include "jellyfin/demo_http.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <thread>

#include "jellyfin/models.h"
#include "jellyfin/text.h"
#include "jellyfin/url.h"

#include "jellyfin/demo_posters.inc"

namespace zerofin::jf {
namespace {

using nlohmann::json;

struct DemoItem {
  std::string id;
  std::string name;
  std::string type;  // Movie, Series, Season, Episode
  int year = 0;
  int runtime_min = 0;
  std::string overview;
  std::string series_id, series_name, season_id;
  int index = -1;
  int parent_index = -1;
  bool played = false;
  double progress = 0;  // fraction of the runtime already watched
  int variant = 0;      // which media streams the file has
  std::string library;  // lib-movies, lib-tv or lib-anime
};

const char* const kMovies[] = {
    "El Orfanato", "Amélie", "La vida es bella", "Spirited Away", "The Dark Knight Rises", "Heat",
    "Blade Runner 2049", "Cinema Paradiso", "El laberinto del fauno", "Parasite", "Whiplash",
    "Super Mario Galaxy: la película", "Mad Max: Fury Road",
    "Il buono, il brutto, il cattivo (Edición Extendida del Director)", "Her", "Arrival", "Coco", "Up", "Wall-E",
    "Ratatouille", "Inside Out", "Interstellar", "The Prestige", "Memento", "Oldboy", "Seven Samurai", "Ikiru",
    "Tokyo Story", "Nausicaä del Valle del Viento", "Princess Mononoke", "Perfect Blue", "Akira",
    "Ghost in the Shell", "Your Name", "Weathering with You", "A Silent Voice", "Grave of the Fireflies", "Paprika",
    "Millennium Actress", "Summer Wars"};

const char* const kSeries[] = {"Dragon Ball", "Fleabag", "The Apothecary Diaries", "Breaking Bad", "Chernobyl",
                               "Mr. Robot"};

struct DemoData {
  std::vector<DemoItem> movies;
  std::vector<DemoItem> series;
  std::vector<DemoItem> seasons;
  std::vector<DemoItem> episodes;

  DemoData() {
    int n = 0;
    for (const char* name : kMovies) {
      ++n;
      DemoItem m;
      m.id = "m" + std::to_string(n);
      m.name = name;
      m.type = "Movie";
      m.year = 1960 + (n * 7) % 62;
      m.runtime_min = 80 + (n * 13) % 70;
      m.overview = std::string("A film that is mostly here to test how the screen wraps and clips a long description of ") +
                   name + ". It keeps going for a few lines so the overview box has to cut it.";
      m.variant = n % 4;
      m.library = "lib-movies";
      if (n == 12) m.progress = 0.61;
      if (n == 3 || n == 7) m.played = true;
      movies.push_back(m);
    }
    int s = 0;
    for (const char* name : kSeries) {
      ++s;
      DemoItem series;
      series.id = "s" + std::to_string(s);
      series.name = name;
      series.type = "Series";
      series.year = 1990 + s * 4;
      series.library = (s == 1 || s == 3) ? "lib-anime" : "lib-tv";
      series.overview = std::string(name) + " is a made-up series for the demo.";
      series.variant = s % 4;
      series.series_name = name;
      series.series_id = series.id;
      this->series.push_back(series);
      for (int season = 1; season <= 3; ++season) {
        DemoItem se;
        se.id = series.id + "-se" + std::to_string(season);
        se.name = "Season " + std::to_string(season);
        se.type = "Season";
        se.series_id = series.id;
        se.series_name = name;
        se.index = season;
        seasons.push_back(se);
        for (int ep = 1; ep <= 8; ++ep) {
          DemoItem e;
          e.id = se.id + "-e" + std::to_string(ep);
          e.name = ep == 3 ? "The one with a very long episode title that cannot fit on one line" : "Episode " + std::to_string(ep);
          e.type = "Episode";
          e.series_id = series.id;
          e.series_name = name;
          e.season_id = se.id;
          e.index = ep;
          e.parent_index = season;
          e.runtime_min = 24 + (ep * 3) % 20;
          e.overview = "Episode " + std::to_string(ep) + " of " + se.name + ".";
          e.variant = (s + ep) % 4;
          e.played = season == 1 || (season == 2 && ep < 3);
          if (s == 1 && season == 2 && ep == 3) e.progress = 0.34;  // Dragon Ball S2E3 is half watched
          episodes.push_back(e);
        }
      }
    }
  }
};

const DemoData& data() {
  static const DemoData d;
  return d;
}

json streams_for(int variant) {
  json s = json::array();
  auto video = [&](const char* codec, const char* profile, int w, int h, int depth, double fps) {
    s.push_back({{"Index", 0}, {"Type", "Video"}, {"Codec", codec}, {"Profile", profile}, {"Width", w},
                 {"Height", h}, {"BitDepth", depth}, {"RealFrameRate", fps}, {"IsInterlaced", false}});
  };
  auto audio = [&](int index, const char* codec, const char* lang, int ch, bool def) {
    s.push_back({{"Index", index}, {"Type", "Audio"}, {"Codec", codec}, {"Language", lang}, {"Channels", ch},
                 {"IsDefault", def}});
  };
  auto sub = [&](int index, const char* codec, const char* lang, bool text) {
    s.push_back({{"Index", index}, {"Type", "Subtitle"}, {"Codec", codec}, {"Language", lang},
                 {"IsTextSubtitleStream", text}});
  };
  switch (variant) {
    case 0:
      video("h264", "Main", 1920, 1080, 8, 23.976);
      audio(1, "aac", "eng", 2, true);
      audio(2, "aac", "jpn", 2, false);
      sub(3, "subrip", "eng", true);
      sub(4, "subrip", "spa", true);
      break;
    case 1:
      video("h264", "High", 1920, 1080, 8, 23.976);
      audio(1, "eac3", "eng", 6, true);
      sub(2, "PGSSUB", "eng", false);
      break;
    case 2:
      video("hevc", "Main 10", 1920, 1080, 10, 23.976);
      audio(1, "aac", "jpn", 2, true);
      sub(2, "subrip", "eng", true);
      break;
    default:
      video("mpeg4", "Advanced Simple Profile", 720, 544, 8, 25);
      audio(1, "mp3", "spa", 2, true);
      break;
  }
  return s;
}

int64_t bitrate_for(int variant) {
  static const int64_t kBitrates[] = {5'200'000, 8'100'000, 1'800'000, 1'300'000};
  return kBitrates[variant % 4];
}

json item_json(const DemoItem& it, bool with_sources) {
  json j = {{"Id", it.id}, {"Name", it.name}, {"Type", it.type}, {"Overview", it.overview}};
  if (it.year) j["ProductionYear"] = it.year;
  if (!it.series_id.empty() && it.type != "Series") {
    j["SeriesId"] = it.series_id;
    j["SeriesName"] = it.series_name;
  }
  if (!it.season_id.empty()) {
    j["SeasonId"] = it.season_id;
  }
  if (it.index >= 0) j["IndexNumber"] = it.index;
  if (it.parent_index >= 0) j["ParentIndexNumber"] = it.parent_index;
  const bool playable = it.type == "Movie" || it.type == "Episode";
  const int64_t runtime = static_cast<int64_t>(it.runtime_min) * 60 * kTicksPerSecond;
  if (playable) j["RunTimeTicks"] = runtime;
  j["UserData"] = {{"PlaybackPositionTicks", static_cast<int64_t>(runtime * it.progress)},
                   {"Played", it.played},
                   {"PlayCount", it.played ? 1 : 0}};
  // Every movie and series has a poster except m5, which shows the placeholder. An episode uses its series.
  if ((it.type == "Movie" || it.type == "Series") && it.id != "m5") j["ImageTags"] = {{"Primary", "demo"}};
  if (it.type == "Episode" || it.type == "Season") j["SeriesPrimaryImageTag"] = "demo";
  if (playable && with_sources) {
    j["MediaSources"] = json::array({{{"Id", it.id + "-src"},
                                      {"Container", "mkv"},
                                      {"Bitrate", bitrate_for(it.variant)},
                                      {"RunTimeTicks", runtime},
                                      {"MediaStreams", streams_for(it.variant)}}});
  }
  return j;
}

std::string list_json(const std::vector<json>& items, int total, int start = 0) {
  return json{{"Items", items}, {"TotalRecordCount", total}, {"StartIndex", start}}.dump();
}

std::string page_json(const std::vector<const DemoItem*>& all, const std::string& url) {
  int start = 0, limit = 50;
  if (auto s = get_query_param(url, "startIndex")) start = std::atoi(s->c_str());
  if (auto l = get_query_param(url, "limit")) limit = std::atoi(l->c_str());
  std::vector<json> page;
  for (int i = start; i < static_cast<int>(all.size()) && i < start + limit; ++i) page.push_back(item_json(*all[static_cast<size_t>(i)], false));
  return list_json(page, static_cast<int>(all.size()), start);
}

const DemoItem* find_any(const std::string& id) {
  for (const auto* list : {&data().movies, &data().series, &data().seasons, &data().episodes}) {
    for (const auto& it : *list) {
      if (it.id == id) return &it;
    }
  }
  return nullptr;
}

// A baseline JPEG for the item id, or 404 for m5 and for an unknown id.
HttpResponse poster_for(const std::string& id) {
  if (id == "m5" || !find_any(id)) return HttpResponse{404, {}, {}};
  unsigned sum = 0;
  for (char c : id) sum += static_cast<unsigned char>(c);
  const auto& p = kDemoPosters[sum % (sizeof(kDemoPosters) / sizeof(kDemoPosters[0]))];
  return HttpResponse{200, std::string(reinterpret_cast<const char*>(p.data), p.size), {}};
}

HttpResponse ok(std::string body) { return HttpResponse{200, std::move(body), {}}; }
HttpResponse status(int code) { return HttpResponse{code, {}, {}}; }

}  // namespace

DemoHttpClient::DemoHttpClient() {
  if (const char* v = std::getenv("ZEROFIN_DEMO_LATENCY_MS")) latency_ms_ = std::atoi(v);
  if (const char* v = std::getenv("ZEROFIN_DEMO_QC_POLLS")) approve_after_polls_ = std::atoi(v);
}

HttpResponse DemoHttpClient::send(const HttpRequest& req) {
  if (latency_ms_ > 0) std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms_));

  const auto host = req.url.find("://");
  const auto slash = req.url.find('/', host == std::string::npos ? 0 : host + 3);
  const auto q = req.url.find('?');
  const std::string path = req.url.substr(slash, q == std::string::npos ? std::string::npos : q - slash);
  const std::string route = req.method + " " + path;
  const auto& d = data();

  if (route == "GET /System/Info/Public") {
    return ok(R"({"Id":"demo","ServerName":"Demo Server","Version":"12.4.0"})");
  }
  if (route == "POST /QuickConnect/Initiate") {
    quick_connect_polls_ = 0;
    return ok(R"({"Secret":"demo-secret","Code":"123456","Authenticated":false})");
  }
  if (route == "GET /QuickConnect/Connect") {
    ++quick_connect_polls_;
    return ok(quick_connect_polls_ >= approve_after_polls_ ? R"({"Authenticated":true})" : R"({"Authenticated":false})");
  }
  const std::string auth = R"({"User":{"Id":"u1","Name":"demo"},"AccessToken":"demo-token","ServerId":"demo"})";
  if (route == "POST /Users/AuthenticateWithQuickConnect") return ok(auth);
  if (route == "POST /Users/AuthenticateByName") {
    try {
      const json body = json::parse(req.body);
      if (body.value("Username", "") == "demo" && body.value("Pw", "") == "demo") return ok(auth);
    } catch (const json::exception&) {
    }
    return status(401);
  }
  if (route == "POST /Sessions/Logout" || path.rfind("/Sessions/Playing", 0) == 0) return status(204);

  if (route == "GET /UserViews") {
    std::vector<json> v = {
        {{"Id", "lib-movies"}, {"Name", "Películas"}, {"Type", "CollectionFolder"}, {"CollectionType", "movies"}},
        {{"Id", "lib-tv"}, {"Name", "Series"}, {"Type", "CollectionFolder"}, {"CollectionType", "tvshows"}},
        {{"Id", "lib-anime"}, {"Name", "Anime"}, {"Type", "CollectionFolder"}, {"CollectionType", "tvshows"}},
        {{"Id", "lib-live"}, {"Name", "Live TV"}, {"Type", "UserView"}, {"CollectionType", "livetv"}},
        {{"Id", "lib-lists"}, {"Name", "Playlists"}, {"Type", "UserView"}, {"CollectionType", "playlists"}}};
    return ok(list_json(v, static_cast<int>(v.size())));
  }
  if (route == "GET /UserItems/Resume") {
    std::vector<json> v;
    for (const auto& e : d.episodes) {
      if (e.id == "s1-se2-e3") v.push_back(item_json(e, false));
    }
    for (const auto& m : d.movies) {
      if (m.progress > 0) v.push_back(item_json(m, false));
    }
    return ok(list_json(v, 0));  // like the real server with enableTotalRecordCount=false
  }
  if (route == "GET /Shows/NextUp") {
    std::vector<json> v;
    for (const char* id : {"s2-se2-e4", "s3-se2-e5", "s5-se3-e1"}) v.push_back(item_json(*find_any(id), false));
    return ok(list_json(v, 0));
  }
  if (route == "GET /Items/Latest") {
    // The newest items of a library: the last ones in the demo data first.
    const std::string parent = get_query_param(req.url, "parentId").value_or("");
    int limit = 16;
    if (auto l = get_query_param(req.url, "limit")) limit = std::atoi(l->c_str());
    std::vector<const DemoItem*> all;
    for (const auto& m : d.movies) {
      if (parent == m.library) all.push_back(&m);
    }
    for (const auto& s : d.series) {
      if (parent == s.library) all.push_back(&s);
    }
    std::vector<json> v;
    for (auto it = all.rbegin(); it != all.rend() && static_cast<int>(v.size()) < limit; ++it) v.push_back(item_json(**it, false));
    return ok(json(v).dump());
  }
  if (req.method == "GET" && path.rfind("/Items/", 0) == 0 && path.size() > 21 &&
      path.compare(path.size() - 15, 15, "/Images/Primary") == 0) {
    return poster_for(path.substr(7, path.size() - 7 - 15));
  }
  if (route == "GET /Items") {
    if (auto ids = get_query_param(req.url, "ids")) {
      std::vector<json> v;
      if (const DemoItem* it = find_any(*ids)) v.push_back(item_json(*it, true));
      return ok(list_json(v, static_cast<int>(v.size())));
    }
    if (auto term = get_query_param(req.url, "searchTerm")) {
      std::vector<const DemoItem*> hits;
      const std::string t = to_lower(*term);
      for (const auto& m : d.movies) {
        if (to_lower(m.name).find(t) != std::string::npos) hits.push_back(&m);
      }
      for (const auto& s : d.series) {
        if (to_lower(s.name).find(t) != std::string::npos) hits.push_back(&s);
      }
      return ok(page_json(hits, req.url));
    }
    const std::string parent = get_query_param(req.url, "parentId").value_or("");
    std::vector<const DemoItem*> all;
    for (const auto& m : d.movies) {
      if (parent.empty() || parent == m.library) all.push_back(&m);
    }
    for (const auto& s : d.series) {
      if (parent.empty() || parent == s.library) all.push_back(&s);
    }
    return ok(page_json(all, req.url));
  }
  if (path.rfind("/Shows/", 0) == 0) {
    const auto rest = path.substr(7);
    const auto sl = rest.find('/');
    const std::string series_id = rest.substr(0, sl);
    const std::string what = sl == std::string::npos ? "" : rest.substr(sl + 1);
    std::vector<json> v;
    if (what == "Seasons") {
      for (const auto& se : d.seasons) {
        if (se.series_id == series_id) v.push_back(item_json(se, false));
      }
    } else if (what == "Episodes") {
      const std::string season = get_query_param(req.url, "seasonId").value_or("");
      for (const auto& e : d.episodes) {
        if (e.series_id == series_id && (season.empty() || e.season_id == season)) v.push_back(item_json(e, false));
      }
    }
    return ok(list_json(v, static_cast<int>(v.size())));
  }
  if (req.method == "POST" && path.rfind("/Items/", 0) == 0 && path.size() > 13 &&
      path.compare(path.size() - 13, 13, "/PlaybackInfo") == 0) {
    const std::string id = path.substr(7, path.size() - 7 - 13);
    const DemoItem* it = find_any(id);
    if (!it) return status(404);
    const std::string src = id + "-src";
    json s = {{"Id", src}, {"Container", "mkv"}};
    s["TranscodingUrl"] = "/videos/" + id + "/master.m3u8?MediaSourceId=" + src + "&ApiKey=demo-token&MaxWidth=1920";
    return ok(json{{"PlaySessionId", "demo-session"}, {"MediaSources", json::array({s})}}.dump());
  }
  return status(404);
}

}  // namespace zerofin::jf
