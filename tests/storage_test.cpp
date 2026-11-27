#include <doctest/doctest.h>

#include <sys/stat.h>

#include <cstdlib>
#include <fstream>

#include "jellyfin/auth.h"
#include "storage/config_store.h"

using namespace zerofin;
using namespace zerofin::storage;

namespace {

struct TempDir {
  std::filesystem::path path = std::filesystem::temp_directory_path() / ("zerofin_test_" + jf::random_hex_id());
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

mode_t mode_of(const std::filesystem::path& p) {
  struct stat st {};
  ::stat(p.c_str(), &st);
  return st.st_mode & 0777;
}

AppConfig sample() {
  AppConfig c;
  c.server_url = "http://192.168.1.157:8096";
  c.server_name = "jellyPi";
  c.token = "tok";
  c.user_id = "u1";
  c.user_name = "admin";
  c.device_id = "dev1";
  c.play_mode = "transcode";
  c.video_output = "lcd";
  c.hdmi_quality = "1080";
  c.hdmi_sound = "speaker";
  return c;
}

}  // namespace

TEST_CASE("a missing config file gives defaults") {
  TempDir dir;
  auto r = ConfigStore(dir.path).load();
  REQUIRE(r.ok());
  CHECK_FALSE(r.value().logged_in());
  CHECK(r.value().play_mode == "auto");
  CHECK(r.value().video_output == "auto");
  CHECK(r.value().hdmi_quality == "720");
  CHECK(r.value().hdmi_sound == "monitor");
  CHECK(r.value().token.empty());
}

TEST_CASE("save then load returns the same values") {
  TempDir dir;
  ConfigStore store(dir.path / "nested" / "ZeroFin");
  REQUIRE(store.save(sample()).ok());
  auto r = store.load();
  REQUIRE(r.ok());
  CHECK(r.value().server_url == "http://192.168.1.157:8096");
  CHECK(r.value().server_name == "jellyPi");
  CHECK(r.value().token == "tok");
  CHECK(r.value().user_id == "u1");
  CHECK(r.value().user_name == "admin");
  CHECK(r.value().device_id == "dev1");
  CHECK(r.value().play_mode == "transcode");
  CHECK(r.value().video_output == "lcd");
  CHECK(r.value().hdmi_quality == "1080");
  CHECK(r.value().hdmi_sound == "speaker");
  CHECK(r.value().logged_in());
}

TEST_CASE("the config file is private to the user") {
  TempDir dir;
  ConfigStore store(dir.path / "ZeroFin");
  REQUIRE(store.save(sample()).ok());
  CHECK(mode_of(store.file_path()) == 0600);
  CHECK(mode_of(store.dir()) == 0700);
}

TEST_CASE("a second save replaces the first and leaves no temp file") {
  TempDir dir;
  ConfigStore store(dir.path);
  REQUIRE(store.save(sample()).ok());
  AppConfig changed = sample();
  changed.token = "other";
  REQUIRE(store.save(changed).ok());
  CHECK(store.load().value().token == "other");
  CHECK_FALSE(std::filesystem::exists(dir.path / "config.json.tmp"));
}

TEST_CASE("a damaged file gives a Parse error") {
  TempDir dir;
  std::filesystem::create_directories(dir.path);
  std::ofstream(dir.path / "config.json") << "{ not json";
  auto r = ConfigStore(dir.path).load();
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().kind == jf::ErrorKind::Parse);
}

TEST_CASE("a JSON array is not a config") {
  TempDir dir;
  std::filesystem::create_directories(dir.path);
  std::ofstream(dir.path / "config.json") << "[]";
  CHECK_FALSE(ConfigStore(dir.path).load().ok());
}

TEST_CASE("unknown keys are ignored and a bad play mode becomes auto") {
  TempDir dir;
  std::filesystem::create_directories(dir.path);
  std::ofstream(dir.path / "config.json") << R"({"token":"t","user_id":"u","server_url":"http://h","play_mode":"fast","future":1})";
  auto r = ConfigStore(dir.path).load();
  REQUIRE(r.ok());
  CHECK(r.value().play_mode == "auto");
  CHECK(r.value().logged_in());
}

TEST_CASE("logged_in needs a server, a token and a user") {
  AppConfig c;
  CHECK_FALSE(c.logged_in());
  c.server_url = "http://h";
  c.token = "t";
  CHECK_FALSE(c.logged_in());
  c.user_id = "u";
  CHECK(c.logged_in());
}

TEST_CASE("default_config_dir follows ZEROFIN_HOME, then HOME") {
  const char* old_zero = std::getenv("ZEROFIN_HOME");
  const char* old_home = std::getenv("HOME");
  const std::string saved_zero = old_zero ? old_zero : "";
  const std::string saved_home = old_home ? old_home : "";

  ::setenv("ZEROFIN_HOME", "/x/y", 1);
  CHECK(default_config_dir() == "/x/y");
  ::unsetenv("ZEROFIN_HOME");
  ::setenv("HOME", "/h", 1);
  CHECK(default_config_dir() == "/h/.config/ZeroFin");

  if (old_zero) ::setenv("ZEROFIN_HOME", saved_zero.c_str(), 1);
  if (old_home) ::setenv("HOME", saved_home.c_str(), 1);
}

TEST_CASE("bad video output, quality and sound values become the defaults") {
  TempDir dir;
  std::filesystem::create_directories(dir.path);
  std::ofstream(dir.path / "config.json")
      << R"({"token":"t","user_id":"u","server_url":"http://h","video_output":"vga","hdmi_quality":"4k","hdmi_sound":"loud"})";
  auto r = ConfigStore(dir.path).load();
  REQUIRE(r.ok());
  CHECK(r.value().video_output == "auto");
  CHECK(r.value().hdmi_quality == "720");
  CHECK(r.value().hdmi_sound == "monitor");
}

TEST_CASE("an old config file with none of the new keys loads with the defaults") {
  TempDir dir;
  std::filesystem::create_directories(dir.path);
  std::ofstream(dir.path / "config.json") << R"({"token":"t","user_id":"u","server_url":"http://h","play_mode":"transcode"})";
  auto r = ConfigStore(dir.path).load();
  REQUIRE(r.ok());
  CHECK(r.value().play_mode == "transcode");
  CHECK(r.value().video_output == "auto");
  CHECK(r.value().hdmi_quality == "720");
}

// ---- saved servers ----

namespace {

SavedServer saved(const char* url, const char* name, const char* token = "", const char* user_id = "",
                  const char* user = "") {
  return SavedServer{url, name, token, user_id, user};
}

}  // namespace

TEST_CASE("saved servers survive a save and a load, in order") {
  TempDir dir;
  AppConfig c = sample();
  c.servers.push_back(saved("https://jf.example.com", "Public", "t2", "u2", "ana"));
  c.servers.push_back(saved("http://10.0.0.5:8096", "Office"));
  REQUIRE(ConfigStore(dir.path).save(c).ok());
  auto r = ConfigStore(dir.path).load();
  REQUIRE(r.ok());
  REQUIRE(r.value().servers.size() == 2);
  CHECK(r.value().servers[0].url == "https://jf.example.com");
  CHECK(r.value().servers[0].name == "Public");
  CHECK(r.value().servers[0].token == "t2");
  CHECK(r.value().servers[0].user_id == "u2");
  CHECK(r.value().servers[0].user_name == "ana");
  CHECK(r.value().servers[1].url == "http://10.0.0.5:8096");
  CHECK(r.value().servers[1].token.empty());
  CHECK(r.value().server_url == "http://192.168.1.157:8096");
}

TEST_CASE("a config file with one server and no list loads with an empty list") {
  TempDir dir;
  REQUIRE(ConfigStore(dir.path).save(sample()).ok());
  auto r = ConfigStore(dir.path).load();
  REQUIRE(r.ok());
  CHECK(r.value().servers.empty());
  CHECK(r.value().logged_in());
}

TEST_CASE("bad entries in the saved server list are skipped") {
  TempDir dir;
  std::filesystem::create_directories(dir.path);
  std::ofstream(dir.path / "config.json")
      << R"({"server_url":"http://a","servers":[1,{"name":"no url"},{"url":"http://b","name":"B"},"x"]})";
  auto r = ConfigStore(dir.path).load();
  REQUIRE(r.ok());
  REQUIRE(r.value().servers.size() == 1);
  CHECK(r.value().servers[0].url == "http://b");
}

TEST_CASE("same_server_address ignores a trailing slash and the case of the host") {
  CHECK(same_server_address("https://JF.example.com/", "https://jf.example.com"));
  CHECK(same_server_address("http://h:8096", "http://h:8096/"));
  CHECK_FALSE(same_server_address("http://h:8096", "https://h:8096"));
  CHECK_FALSE(same_server_address("http://h:8096", "http://h"));
  CHECK_FALSE(same_server_address("", ""));
}

TEST_CASE("stash_active_server moves the active login to the front of the list") {
  AppConfig c = sample();
  c.servers.push_back(saved("https://old.example", "Old"));
  stash_active_server(c);
  CHECK(c.server_url.empty());
  CHECK(c.token.empty());
  CHECK(c.user_id.empty());
  CHECK(c.user_name.empty());
  CHECK(c.server_name.empty());
  REQUIRE(c.servers.size() == 2);
  CHECK(c.servers[0].url == "http://192.168.1.157:8096");
  CHECK(c.servers[0].name == "jellyPi");
  CHECK(c.servers[0].token == "tok");
  CHECK(c.servers[0].user_name == "admin");
  CHECK(c.servers[1].url == "https://old.example");
}

TEST_CASE("stash_active_server does nothing when there is no active server") {
  AppConfig c;
  c.servers.push_back(saved("https://old.example", "Old"));
  stash_active_server(c);
  CHECK(c.servers.size() == 1);
}

TEST_CASE("stash_active_server replaces a list entry with the same address") {
  AppConfig c = sample();
  c.servers.push_back(saved("http://192.168.1.157:8096/", "stale", "oldtok", "u0", "someone"));
  stash_active_server(c);
  REQUIRE(c.servers.size() == 1);
  CHECK(c.servers[0].token == "tok");
}

TEST_CASE("activate_saved_server swaps the saved login with the active one") {
  AppConfig c = sample();
  c.servers.push_back(saved("https://jf.example.com", "Public", "t2", "u2", "ana"));
  REQUIRE(activate_saved_server(c, "https://jf.example.com/"));
  CHECK(c.server_url == "https://jf.example.com");
  CHECK(c.server_name == "Public");
  CHECK(c.token == "t2");
  CHECK(c.user_id == "u2");
  CHECK(c.user_name == "ana");
  CHECK(c.logged_in());
  REQUIRE(c.servers.size() == 1);
  CHECK(c.servers[0].url == "http://192.168.1.157:8096");
  CHECK(c.servers[0].token == "tok");
}

TEST_CASE("activate_saved_server with an unknown address changes nothing") {
  AppConfig c = sample();
  c.servers.push_back(saved("https://jf.example.com", "Public"));
  CHECK_FALSE(activate_saved_server(c, "https://nope.example"));
  CHECK(c.server_url == "http://192.168.1.157:8096");
  CHECK(c.token == "tok");
  CHECK(c.servers.size() == 1);
}

TEST_CASE("activate_saved_server on a signed out server leaves the active fields signed out") {
  AppConfig c = sample();
  c.servers.push_back(saved("https://jf.example.com", "Public"));
  REQUIRE(activate_saved_server(c, "https://jf.example.com"));
  CHECK_FALSE(c.logged_in());
  CHECK(c.server_url == "https://jf.example.com");
}

TEST_CASE("remove_saved_server drops one entry and reports whether it was there") {
  AppConfig c = sample();
  c.servers.push_back(saved("https://a.example", "A"));
  c.servers.push_back(saved("https://b.example", "B"));
  CHECK(remove_saved_server(c, "https://a.example/"));
  REQUIRE(c.servers.size() == 1);
  CHECK(c.servers[0].url == "https://b.example");
  CHECK_FALSE(remove_saved_server(c, "https://a.example"));
  CHECK(c.server_url == "http://192.168.1.157:8096");
}

TEST_CASE("select_server keeps the login when the address is the active server") {
  AppConfig c = sample();
  select_server(c, "http://192.168.1.157:8096/", "New name");
  CHECK(c.server_url == "http://192.168.1.157:8096");
  CHECK(c.server_name == "New name");
  CHECK(c.token == "tok");
  CHECK(c.servers.empty());
}

TEST_CASE("select_server brings back a saved server with its login") {
  AppConfig c = sample();
  c.servers.push_back(saved("https://jf.example.com", "Public", "t2", "u2", "ana"));
  select_server(c, "https://jf.example.com", "Public renamed");
  CHECK(c.server_url == "https://jf.example.com");
  CHECK(c.server_name == "Public renamed");
  CHECK(c.token == "t2");
  CHECK(c.user_name == "ana");
  REQUIRE(c.servers.size() == 1);
  CHECK(c.servers[0].token == "tok");
}

TEST_CASE("select_server with a new address saves the active server and starts signed out") {
  AppConfig c = sample();
  select_server(c, "https://new.example", "New");
  CHECK(c.server_url == "https://new.example");
  CHECK(c.server_name == "New");
  CHECK_FALSE(c.logged_in());
  CHECK(c.token.empty());
  REQUIRE(c.servers.size() == 1);
  CHECK(c.servers[0].url == "http://192.168.1.157:8096");
  CHECK(c.servers[0].token == "tok");
}

TEST_CASE("select_server on a first run has nothing to save") {
  AppConfig c;
  select_server(c, "https://new.example", "New");
  CHECK(c.server_url == "https://new.example");
  CHECK(c.servers.empty());
}
