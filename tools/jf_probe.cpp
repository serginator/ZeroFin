// Live check of the Jellyfin client library against a real server.
//
// Usage: zerofin_jf_probe SERVER_URL [--item ID] [--sub INDEX] [--transcode] [--cli-curl]
//
// Logs in with Quick Connect (it prints a code to enter in a logged-in Jellyfin client),
// lists views, resume and next-up items, prepares playback for one item, and logs out
// at the end so the token is revoked. It never changes watch progress. Tokens are redacted.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "jellyfin/auth.h"
#include "jellyfin/client.h"
#include "jellyfin/curl_cli_http.h"
#include "jellyfin/curl_http.h"
#include "jellyfin/url.h"

using namespace zerofin::jf;

namespace {

template <typename T>
bool check(const Result<T>& r, const char* what) {
  if (r) return true;
  std::fprintf(stderr, "FAILED %s: %s (kind %d, HTTP %d)\n", what, r.error().message.c_str(),
               static_cast<int>(r.error().kind), r.error().http_status);
  return false;
}

void print_items(const char* title, const ItemList& list, size_t max = 5) {
  std::printf("%s: %zu shown, %d total\n", title, list.items.size(), list.total);
  for (size_t i = 0; i < list.items.size() && i < max; ++i) {
    const Item& it = list.items[i];
    std::printf("  - [%s] %s%s%s  (id %s)\n", it.raw_type.c_str(), it.name.c_str(),
                it.series_name.empty() ? "" : "  / ", it.series_name.c_str(), it.id.c_str());
  }
}

void print_source(const MediaSource& s) {
  std::printf("media source %s: container %s, %lld kbps\n", s.id.c_str(), s.container.c_str(),
              static_cast<long long>(s.bitrate / 1000));
  for (const auto& st : s.streams) {
    const char* type = st.type == StreamType::Video ? "video" : st.type == StreamType::Audio ? "audio" : "sub  ";
    std::printf("  #%d %s %s %s %dx%d bit%d %s%s\n", st.index, type, st.codec.c_str(), st.profile.c_str(), st.width,
                st.height, st.bit_depth, st.language.c_str(), st.is_default ? " (default)" : "");
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::string server;
  std::string item_id;
  std::optional<int> sub;
  bool force_transcode = false;
  bool cli_curl = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--cli-curl") {
      cli_curl = true;
    } else if (a == "--item" && i + 1 < argc) {
      item_id = argv[++i];
    } else if (a == "--sub" && i + 1 < argc) {
      sub = std::atoi(argv[++i]);
    } else if (a == "--transcode") {
      force_transcode = true;
    } else if (server.empty()) {
      server = a;
    }
  }
  auto base = normalize_server_url(server);
  if (!base) {
    std::fprintf(stderr, "usage: %s SERVER_URL [--item ID] [--sub INDEX] [--transcode] [--cli-curl]\n", argv[0]);
    return 2;
  }

  CurlHttpClient libcurl_http;
  CurlCliHttpClient cli_http;
  HttpClient& http = cli_curl ? static_cast<HttpClient&>(cli_http) : static_cast<HttpClient&>(libcurl_http);
  ClientInfo info;
  info.device_id = random_hex_id();
  Client client(http, info, *base);

  auto server_info = client.public_info();
  if (!check(server_info, "public_info")) return 1;
  std::printf("server: %s %s (id %s)\n", server_info.value().name.c_str(), server_info.value().version.c_str(),
              server_info.value().id.c_str());

  auto qc = client.quick_connect_initiate();
  if (!check(qc, "quick_connect_initiate")) return 1;
  std::printf("QUICK CONNECT CODE: %s\n", qc.value().code.c_str());
  std::fflush(stdout);

  bool authorized = false;
  for (int i = 0; i < 90 && !authorized; ++i) {
    auto state = client.quick_connect_authorized(qc.value().secret);
    if (!check(state, "quick_connect_authorized")) return 1;
    authorized = state.value();
    if (!authorized) std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  if (!authorized) {
    std::fprintf(stderr, "timed out waiting for approval\n");
    return 1;
  }
  auto auth = client.login_quick_connect(qc.value().secret);
  if (!check(auth, "login_quick_connect")) return 1;
  std::printf("logged in as %s\n", auth.value().user.name.c_str());

  int status = 0;
  auto views = client.views();
  if (check(views, "views")) print_items("views", views.value());
  auto resume = client.resume_items(3);
  if (check(resume, "resume_items")) print_items("resume", resume.value());
  auto next_up = client.next_up(3);
  if (check(next_up, "next_up")) print_items("next up", next_up.value());
  auto search = client.search("the", 3);
  if (check(search, "search")) print_items("search 'the'", search.value(), 3);

  if (item_id.empty() && resume && !resume.value().items.empty()) item_id = resume.value().items[0].id;
  if (!item_id.empty()) {
    auto item = client.item(item_id);
    if (check(item, "item") && !item.value().media_sources.empty()) {
      std::printf("item: %s\n", item.value().name.c_str());
      const MediaSource& source = item.value().media_sources[0];
      print_source(source);

      PlayRequest req;
      req.item_id = item_id;
      req.source = &source;
      req.subtitle_index = sub;
      req.mode = force_transcode ? PlayMode::AlwaysTranscode : PlayMode::Auto;
      req.start_ticks = 0;
      auto plan = client.prepare_playback(req);
      if (check(plan, "prepare_playback")) {
        const PlaybackPlan& p = plan.value();
        std::printf("plan: %s, reason %d, session %s, audio #%d, subtitle #%d%s\n",
                    p.method == Method::DirectPlay ? "DIRECT PLAY" : "TRANSCODE", static_cast<int>(p.reason),
                    p.play_session_id.c_str(), p.audio_index, p.subtitle_index,
                    p.subtitle_burned_in ? " (burned in)" : "");
        std::printf("url: %s\n", redact_url(p.url).c_str());
        if (p.external_subtitle_url) std::printf("subtitle: %s\n", redact_url(*p.external_subtitle_url).c_str());
      } else {
        status = 1;
      }
    } else {
      status = 1;
    }
  }

  auto out = client.logout();
  std::printf("logout: %s\n", out ? "ok" : out.error().message.c_str());
  auto after = client.views();
  std::printf("views after logout: %s\n", after ? "STILL WORKS (token not revoked)" : "refused, as expected");
  return status;
}
