#include <doctest/doctest.h>

#include <map>
#include <nlohmann/json.hpp>

#include "fixtures.h"
#include "jellyfin/client.h"
#include "jellyfin/url.h"

using namespace zerofin::jf;
using nlohmann::json;

namespace {

// Records every request and answers from a table keyed by "METHOD /path".
class FakeHttp : public HttpClient {
 public:
  std::vector<HttpRequest> requests;
  std::map<std::string, HttpResponse> routes;
  bool transport_fails = false;

  void on(const std::string& method_and_path, int status, std::string body = {}) {
    routes[method_and_path] = HttpResponse{status, std::move(body), {}};
  }

  HttpResponse send(const HttpRequest& request) override {
    requests.push_back(request);
    if (transport_fails) return HttpResponse{0, {}, "could not connect"};
    auto it = routes.find(request.method + " " + path_of(request.url));
    return it == routes.end() ? HttpResponse{404, {}, {}} : it->second;
  }

  const HttpRequest& last() const { return requests.back(); }

  static std::string path_of(const std::string& url) {
    const auto host = url.find("://") + 3;
    const auto slash = url.find('/', host);
    const auto query = url.find('?');
    return url.substr(slash, query == std::string::npos ? std::string::npos : query - slash);
  }
};

ClientInfo test_info() {
  ClientInfo info;
  info.device_id = "dev1";
  return info;
}

std::string header(const HttpRequest& r, const std::string& name) {
  for (const auto& h : r.headers) {
    if (h.name == name) return h.value;
  }
  return {};
}

struct Fixture {
  FakeHttp http;
  Client client{http, test_info(), "http://srv:8096"};
  Fixture() { client.set_session(Session{"tok", "u1"}); }
};

Item movie(long long bitrate = 5'200'000) {
  return parse_item_list(fixtures::items_json(bitrate)).value().items[0];
}

}  // namespace

TEST_CASE("public_info sends the client header and no token") {
  Fixture f;
  f.client.set_session({});
  f.http.on("GET /System/Info/Public", 200, R"({"Id":"abc","ServerName":"jellyPi","Version":"12.4.0"})");
  auto info = f.client.public_info();
  REQUIRE(info.ok());
  CHECK(info.value().name == "jellyPi");
  CHECK(f.http.last().url == "http://srv:8096/System/Info/Public");
  const std::string auth = header(f.http.last(), "Authorization");
  CHECK(auth.find("Client=\"ZeroFin\"") != std::string::npos);
  CHECK(auth.find("DeviceId=\"dev1\"") != std::string::npos);
  CHECK(auth.find("Token=") == std::string::npos);
}

TEST_CASE("Quick Connect flow ends with a session that sends the token") {
  FakeHttp http;
  Client client(http, test_info(), "http://srv:8096");
  http.on("POST /QuickConnect/Initiate", 200, R"({"Secret":"sec","Code":"123456"})");
  http.on("GET /QuickConnect/Connect", 200, R"({"Authenticated":false})");
  http.on("POST /Users/AuthenticateWithQuickConnect", 200, fixtures::auth_json());
  http.on("GET /UserViews", 200, R"({"Items":[]})");

  auto session = client.quick_connect_initiate();
  REQUIRE(session.ok());
  CHECK(session.value().code == "123456");

  auto waiting = client.quick_connect_authorized("sec");
  REQUIRE(waiting.ok());
  CHECK_FALSE(waiting.value());
  CHECK(get_query_param(http.last().url, "secret") == "sec");

  http.on("GET /QuickConnect/Connect", 200, R"({"Authenticated":true})");
  CHECK(client.quick_connect_authorized("sec").value());

  auto auth = client.login_quick_connect("sec");
  REQUIRE(auth.ok());
  CHECK(json::parse(http.last().body)["Secret"] == "sec");
  CHECK(client.session().token == "tok");
  CHECK(client.session().user_id == "u1");

  REQUIRE(client.views().ok());
  CHECK(header(http.last(), "Authorization").find("Token=\"tok\"") != std::string::npos);
  CHECK(get_query_param(http.last().url, "userId") == "u1");
}

TEST_CASE("login_password sends the credentials in the body, not in the URL") {
  FakeHttp http;
  Client client(http, test_info(), "http://srv:8096");
  http.on("POST /Users/AuthenticateByName", 200, fixtures::auth_json());
  REQUIRE(client.login_password("admin", "p@ss word").ok());
  const json body = json::parse(http.last().body);
  CHECK(body["Username"] == "admin");
  CHECK(body["Pw"] == "p@ss word");
  CHECK(http.last().url.find("p@ss") == std::string::npos);
  CHECK(header(http.last(), "Content-Type") == "application/json");
}

TEST_CASE("a rejected login returns Unauthorized and sets no session") {
  FakeHttp http;
  Client client(http, test_info(), "http://srv:8096");
  http.on("POST /Users/AuthenticateByName", 401);
  auto r = client.login_password("admin", "wrong");
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().kind == ErrorKind::Unauthorized);
  CHECK_FALSE(client.session().logged_in());
}

TEST_CASE("transport failures return a Network error") {
  Fixture f;
  f.http.transport_fails = true;
  auto r = f.client.resume_items();
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().kind == ErrorKind::Network);
  CHECK(r.error().message == "could not connect");
}

TEST_CASE("server errors map to error kinds") {
  Fixture f;
  f.http.on("GET /UserViews", 500);
  CHECK(f.client.views().error().kind == ErrorKind::Http);
  CHECK(f.client.views().error().http_status == 500);
  f.http.on("GET /UserViews", 401);
  CHECK(f.client.views().error().kind == ErrorKind::Unauthorized);
}

TEST_CASE("a redirect that was not followed is an error, not an empty success") {
  Fixture f;
  f.http.on("GET /UserViews", 302);
  const auto r = f.client.views();
  REQUIRE_FALSE(r);
  CHECK(r.error().kind == ErrorKind::Http);
  CHECK(r.error().http_status == 302);
}

TEST_CASE("browse calls need a login and make no request without one") {
  FakeHttp http;
  Client client(http, test_info(), "http://srv:8096");
  CHECK(client.views().error().kind == ErrorKind::Unauthorized);
  CHECK(client.resume_items().error().kind == ErrorKind::Unauthorized);
  CHECK(client.next_up().error().kind == ErrorKind::Unauthorized);
  CHECK(client.search("x").error().kind == ErrorKind::Unauthorized);
  CHECK(client.item("m1").error().kind == ErrorKind::Unauthorized);
  CHECK(http.requests.empty());
}

TEST_CASE("resume_items and next_up build the right requests") {
  Fixture f;
  f.http.on("GET /UserItems/Resume", 200, fixtures::items_json());
  f.http.on("GET /Shows/NextUp", 200, fixtures::items_json());

  REQUIRE(f.client.resume_items(5).ok());
  CHECK(get_query_param(f.http.last().url, "userId") == "u1");
  CHECK(get_query_param(f.http.last().url, "limit") == "5");
  CHECK(get_query_param(f.http.last().url, "mediaTypes") == "Video");

  REQUIRE(f.client.next_up(8).ok());
  CHECK(FakeHttp::path_of(f.http.last().url) == "/Shows/NextUp");
  CHECK(get_query_param(f.http.last().url, "limit") == "8");
}

TEST_CASE("items builds a library query") {
  Fixture f;
  f.http.on("GET /Items", 200, fixtures::items_json());
  ItemQuery q;
  q.parent_id = "lib1";
  q.include_types = "Movie,Series";
  q.start_index = 50;
  q.limit = 25;
  REQUIRE(f.client.items(q).ok());
  const std::string url = f.http.last().url;
  CHECK(get_query_param(url, "parentId") == "lib1");
  CHECK(get_query_param(url, "includeItemTypes") == "Movie,Series");
  CHECK(get_query_param(url, "recursive") == "true");
  CHECK(get_query_param(url, "startIndex") == "50");
  CHECK(get_query_param(url, "limit") == "25");
  CHECK(get_query_param(url, "sortBy") == "SortName");
}

TEST_CASE("seasons and episodes use the Shows routes") {
  Fixture f;
  f.http.on("GET /Shows/s1/Seasons", 200, fixtures::items_json());
  f.http.on("GET /Shows/s1/Episodes", 200, fixtures::items_json());
  REQUIRE(f.client.seasons("s1").ok());
  REQUIRE(f.client.episodes("s1", "se2").ok());
  CHECK(get_query_param(f.http.last().url, "seasonId") == "se2");
  CHECK(get_query_param(f.http.last().url, "userId") == "u1");
}

TEST_CASE("search encodes the term") {
  Fixture f;
  f.http.on("GET /Items", 200, fixtures::items_json());
  REQUIRE(f.client.search("spider man & co", 10).ok());
  CHECK(f.http.last().url.find("searchTerm=spider%20man%20%26%20co") != std::string::npos);
  CHECK(get_query_param(f.http.last().url, "limit") == "10");
}

TEST_CASE("item asks for media sources and reports a missing item") {
  Fixture f;
  f.http.on("GET /Items", 200, fixtures::items_json());
  auto item = f.client.item("m1");
  REQUIRE(item.ok());
  CHECK(item.value().id == "m1");
  CHECK(get_query_param(f.http.last().url, "ids") == "m1");
  CHECK(get_query_param(f.http.last().url, "fields") == "MediaSources,Overview");

  f.http.on("GET /Items", 200, R"({"Items":[]})");
  CHECK(f.client.item("zzz").error().kind == ErrorKind::NotFound);
}

TEST_CASE("direct play builds a static stream URL and asks the server for direct play") {
  Fixture f;
  f.http.on("POST /Items/m1/PlaybackInfo", 200, fixtures::playback_info_direct_json());
  const Item m = movie();

  PlayRequest req;
  req.item_id = "m1";
  req.source = &m.media_sources[0];
  req.start_ticks = seconds_to_ticks(95);
  auto plan = f.client.prepare_playback(req);
  REQUIRE(plan.ok());

  CHECK(plan.value().method == Method::DirectPlay);
  CHECK(plan.value().url ==
        "http://srv:8096/Videos/m1/stream?static=true&MediaSourceId=ms1&PlaySessionId=ps1&api_key=tok");
  CHECK(plan.value().play_session_id == "ps1");
  CHECK(plan.value().audio_index == 1);  // the default audio stream
  CHECK(plan.value().subtitle_index == -1);
  CHECK_FALSE(plan.value().external_subtitle_url.has_value());

  const std::string url = f.http.last().url;
  CHECK(get_query_param(url, "enableDirectPlay") == "true");
  CHECK(get_query_param(url, "enableTranscoding") == "false");
  CHECK(get_query_param(url, "subtitleStreamIndex") == "-1");
  CHECK(get_query_param(url, "startTimeTicks") == "950000000");
  CHECK(get_query_param(url, "maxStreamingBitrate") == "6000000");
  const json body = json::parse(f.http.last().body);
  CHECK(body["DeviceProfile"]["DirectPlayProfiles"].size() == 1);
}

TEST_CASE("a text subtitle on direct play gives an external subtitle URL") {
  Fixture f;
  f.http.on("POST /Items/m1/PlaybackInfo", 200, fixtures::playback_info_direct_json());
  const Item m = movie();
  PlayRequest req;
  req.item_id = "m1";
  req.source = &m.media_sources[0];
  req.subtitle_index = 2;
  auto plan = f.client.prepare_playback(req);
  REQUIRE(plan.ok());
  CHECK(plan.value().method == Method::DirectPlay);
  CHECK_FALSE(plan.value().subtitle_burned_in);
  CHECK(plan.value().subtitle_index == 2);
  CHECK(plan.value().external_subtitle_url == "http://srv:8096/Videos/m1/ms1/Subtitles/2/0/Stream.srt?api_key=tok");
  CHECK(get_query_param(f.http.last().url, "subtitleStreamIndex") == "-1");  // the server draws nothing
}

TEST_CASE("a bitrate above the cap transcodes and the URL gets the screen limits") {
  Fixture f;
  f.http.on("POST /Items/m1/PlaybackInfo", 200, fixtures::playback_info_transcode_json());
  const Item m = movie(8'100'000);
  PlayRequest req;
  req.item_id = "m1";
  req.source = &m.media_sources[0];
  auto plan = f.client.prepare_playback(req);
  REQUIRE(plan.ok());

  CHECK(plan.value().method == Method::Transcode);
  CHECK(plan.value().reason == Reason::BitrateTooHigh);
  const std::string url = plan.value().url;
  CHECK(url.find("http://srv:8096/videos/m1/master.m3u8?") == 0);
  CHECK(get_query_param(url, "maxWidth") == "320");
  CHECK(get_query_param(url, "maxHeight") == "170");
  CHECK(url.find("MaxWidth=1920") == std::string::npos);  // replaced, not duplicated
  CHECK(get_query_param(url, "SubtitleStreamIndex") == "-1");
  CHECK(get_query_param(url, "VideoBitrate") == "1200000");
  CHECK(get_query_param(url, "AudioBitrate") == "96000");
  CHECK(get_query_param(url, "MaxFramerate") == "24");
  CHECK(get_query_param(url, "api_key") == "tok");
  CHECK(get_query_param(url, "PlaySessionId") == "ps1");

  const std::string request_url = f.http.last().url;
  CHECK(get_query_param(request_url, "enableDirectPlay") == "false");
  CHECK(get_query_param(request_url, "enableTranscoding") == "true");
  CHECK(get_query_param(request_url, "maxStreamingBitrate") == "1296000");
  CHECK(json::parse(f.http.last().body)["DeviceProfile"]["DirectPlayProfiles"].empty());
}

TEST_CASE("the transcoding URL keeps the server's ApiKey and gets no second key") {
  // Jellyfin 12.4 puts ApiKey in the URL it returns.
  Fixture f;
  f.http.on("POST /Items/m1/PlaybackInfo", 200,
            R"({"PlaySessionId":"ps1","MediaSources":[{"Id":"ms1","TranscodingUrl":)"
            R"("/videos/m1/master.m3u8?MediaSourceId=ms1&ApiKey=tok&SubtitleMethod=Encode"}]})");
  const Item m = movie(8'100'000);
  PlayRequest req;
  req.item_id = "m1";
  req.source = &m.media_sources[0];
  auto plan = f.client.prepare_playback(req);
  REQUIRE(plan.ok());
  CHECK(get_query_param(plan.value().url, "ApiKey") == "tok");
  CHECK_FALSE(get_query_param(plan.value().url, "api_key").has_value());
}

TEST_CASE("an image subtitle burns in and keeps its index in the URL") {
  Fixture f;
  f.http.on("POST /Items/m1/PlaybackInfo", 200, fixtures::playback_info_transcode_json());
  const Item m = movie();  // low bitrate: only the subtitle forces the transcode
  PlayRequest req;
  req.item_id = "m1";
  req.source = &m.media_sources[0];
  req.subtitle_index = 3;
  auto plan = f.client.prepare_playback(req);
  REQUIRE(plan.ok());
  CHECK(plan.value().method == Method::Transcode);
  CHECK(plan.value().reason == Reason::ImageSubtitleBurnIn);
  CHECK(plan.value().subtitle_burned_in);
  CHECK(get_query_param(plan.value().url, "SubtitleStreamIndex") == "3");
  CHECK(get_query_param(plan.value().url, "SubtitleMethod") == "Encode");
  CHECK(get_query_param(f.http.last().url, "subtitleStreamIndex") == "3");
  CHECK_FALSE(plan.value().external_subtitle_url.has_value());
}

TEST_CASE("Always transcode sends a transcode-only request even for a direct-play file") {
  Fixture f;
  f.http.on("POST /Items/m1/PlaybackInfo", 200, fixtures::playback_info_transcode_json());
  const Item m = movie();
  PlayRequest req;
  req.item_id = "m1";
  req.source = &m.media_sources[0];
  req.mode = PlayMode::AlwaysTranscode;
  auto plan = f.client.prepare_playback(req);
  REQUIRE(plan.ok());
  CHECK(plan.value().reason == Reason::ForcedByUser);
  CHECK(get_query_param(f.http.last().url, "enableDirectPlay") == "false");
}

TEST_CASE("the chosen audio stream is passed to the server on a transcode") {
  Fixture f;
  f.http.on("POST /Items/m1/PlaybackInfo", 200, fixtures::playback_info_transcode_json());
  const Item m = movie(8'100'000);
  PlayRequest req;
  req.item_id = "m1";
  req.source = &m.media_sources[0];
  req.audio_index = 1;
  auto plan = f.client.prepare_playback(req);
  REQUIRE(plan.ok());
  CHECK(get_query_param(plan.value().url, "AudioStreamIndex") == "1");
  CHECK(get_query_param(f.http.last().url, "audioStreamIndex") == "1");
}

TEST_CASE("a transcode without a transcoding URL is an error") {
  Fixture f;
  f.http.on("POST /Items/m1/PlaybackInfo", 200, fixtures::playback_info_direct_json());
  const Item m = movie(8'100'000);
  PlayRequest req;
  req.item_id = "m1";
  req.source = &m.media_sources[0];
  auto plan = f.client.prepare_playback(req);
  REQUIRE_FALSE(plan.ok());
  CHECK(plan.error().kind == ErrorKind::Http);
}

TEST_CASE("prepare_playback checks its input") {
  Fixture f;
  PlayRequest req;
  req.item_id = "m1";
  CHECK(f.client.prepare_playback(req).error().kind == ErrorKind::Invalid);  // no source
  f.client.set_session({});
  const Item m = movie();
  req.source = &m.media_sources[0];
  CHECK(f.client.prepare_playback(req).error().kind == ErrorKind::Unauthorized);
  CHECK(f.http.requests.empty());
}

TEST_CASE("progress reports carry the position and the play method") {
  Fixture f;
  f.http.on("POST /Sessions/Playing", 204);
  f.http.on("POST /Sessions/Playing/Progress", 204);
  f.http.on("POST /Sessions/Playing/Stopped", 204);

  ProgressReport r;
  r.item_id = "m1";
  r.media_source_id = "ms1";
  r.play_session_id = "ps1";
  r.position_ticks = seconds_to_ticks(120);
  r.method = Method::Transcode;
  r.audio_index = 1;
  r.subtitle_index = -1;

  REQUIRE(f.client.report_start(r).ok());
  json body = json::parse(f.http.last().body);
  CHECK(body["ItemId"] == "m1");
  CHECK(body["PlaySessionId"] == "ps1");
  CHECK(body["PositionTicks"] == 1'200'000'000);
  CHECK(body["PlayMethod"] == "Transcode");
  CHECK(body["AudioStreamIndex"] == 1);
  CHECK_FALSE(body.contains("IsPaused"));

  r.is_paused = true;
  REQUIRE(f.client.report_progress(r).ok());
  CHECK(FakeHttp::path_of(f.http.last().url) == "/Sessions/Playing/Progress");
  CHECK(json::parse(f.http.last().body)["IsPaused"] == true);

  r.method = Method::DirectPlay;
  REQUIRE(f.client.report_stopped(r).ok());
  CHECK(FakeHttp::path_of(f.http.last().url) == "/Sessions/Playing/Stopped");
  CHECK(json::parse(f.http.last().body)["PlayMethod"] == "DirectPlay");
}

TEST_CASE("a failed progress report returns the error") {
  Fixture f;
  f.http.transport_fails = true;
  CHECK(f.client.report_progress(ProgressReport{}).error().kind == ErrorKind::Network);
}

TEST_CASE("logout clears the session on success only") {
  Fixture f;
  f.http.on("POST /Sessions/Logout", 500);
  CHECK_FALSE(f.client.logout().ok());
  CHECK(f.client.session().logged_in());
  f.http.on("POST /Sessions/Logout", 204);
  CHECK(f.client.logout().ok());
  CHECK_FALSE(f.client.session().logged_in());
}

TEST_CASE("image_request builds the poster request") {
  Fixture f;
  const HttpRequest r = f.client.image_request(ImageRef{"m1", "tagA"});
  CHECK(r.method == "GET");
  CHECK(FakeHttp::path_of(r.url) == "/Items/m1/Images/Primary");
  CHECK(get_query_param(r.url, "maxWidth") == "64");
  CHECK(get_query_param(r.url, "maxHeight") == "96");
  CHECK(get_query_param(r.url, "format") == "Jpg");
  CHECK(get_query_param(r.url, "tag") == "tagA");
  // The token goes in the header only. A URL with a token ends up in proxy and server logs.
  CHECK_FALSE(get_query_param(r.url, "api_key").has_value());
  CHECK(header(r, "Authorization").find("tok") != std::string::npos);
  CHECK(r.timeout_ms == 15000);
  CHECK(f.http.requests.empty());
  const HttpRequest bare = f.client.image_request(ImageRef{"m1", ""});
  CHECK_FALSE(get_query_param(bare.url, "tag").has_value());
}

TEST_CASE("subtitle_url needs no request") {
  Fixture f;
  CHECK(f.client.subtitle_url("m1", "ms1", 4, "ass") ==
        "http://srv:8096/Videos/m1/ms1/Subtitles/4/0/Stream.ass?api_key=tok");
  CHECK(f.http.requests.empty());
}

TEST_CASE("latest sends the library and parses an array") {
  Fixture f;
  f.http.on("GET /Items/Latest", 200, R"([{"Id":"m9","Name":"New","Type":"Movie"},{"Id":"s9","Name":"S","Type":"Series"}])");
  auto r = f.client.latest("lib1", 8);
  REQUIRE(r.ok());
  CHECK(r.value().items.size() == 2);
  CHECK(FakeHttp::path_of(f.http.last().url) == "/Items/Latest");
  CHECK(get_query_param(f.http.last().url, "parentId") == "lib1");
  CHECK(get_query_param(f.http.last().url, "limit") == "8");
  CHECK(get_query_param(f.http.last().url, "groupItems") == "true");
  CHECK(get_query_param(f.http.last().url, "userId") == "u1");
}
