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
  return c;
}

}  // namespace

TEST_CASE("a missing config file gives defaults") {
  TempDir dir;
  auto r = ConfigStore(dir.path).load();
  REQUIRE(r.ok());
  CHECK_FALSE(r.value().logged_in());
  CHECK(r.value().play_mode == "auto");
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
