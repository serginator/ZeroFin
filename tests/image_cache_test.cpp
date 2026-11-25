#include <doctest/doctest.h>

#include <chrono>
#include <fstream>

#include "storage/image_cache.h"

using namespace zerofin::storage;
namespace fs = std::filesystem;

namespace {
struct TempDir {
  fs::path path;
  TempDir() {
    path = fs::temp_directory_path() / ("zerofin_cache_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};
}  // namespace

TEST_CASE("put then get returns the bytes") {
  TempDir d;
  ImageCache c(d.path);
  REQUIRE(c.put("a.jpg", "hello"));
  CHECK(c.get("a.jpg") == std::optional<std::string>("hello"));
}

TEST_CASE("get of a missing file is nullopt") {
  TempDir d;
  ImageCache c(d.path);
  CHECK_FALSE(c.get("nope.jpg").has_value());
}

TEST_CASE("file_name replaces unsafe characters and handles an empty tag") {
  CHECK(ImageCache::file_name("m1", "tagA", 64, 96) == "m1-tagA-64x96.jpg");
  CHECK(ImageCache::file_name("../x/y", "", 64, 96) == "___x_y-notag-64x96.jpg");
  CHECK(ImageCache::file_name("s1-se2", "a b", 64, 96) == "s1-se2-a_b-64x96.jpg");
}

TEST_CASE("evict removes the oldest files first") {
  TempDir d;
  ImageCache c(d.path, 20);
  REQUIRE(c.put("old.jpg", std::string(10, 'a')));
  REQUIRE(c.put("mid.jpg", std::string(10, 'b')));
  const auto now = fs::file_time_type::clock::now();
  fs::last_write_time(d.path / "old.jpg", now - std::chrono::hours(3));
  fs::last_write_time(d.path / "mid.jpg", now - std::chrono::hours(2));
  REQUIRE(c.put("new.jpg", std::string(10, 'c')));  // 30 bytes: over the limit, "old" goes
  CHECK_FALSE(fs::exists(d.path / "old.jpg"));
  CHECK(fs::exists(d.path / "mid.jpg"));
  CHECK(fs::exists(d.path / "new.jpg"));
}

TEST_CASE("get refreshes the time so the file survives eviction") {
  TempDir d;
  ImageCache c(d.path, 20);
  REQUIRE(c.put("a.jpg", std::string(10, 'a')));
  REQUIRE(c.put("b.jpg", std::string(10, 'b')));
  const auto now = fs::file_time_type::clock::now();
  fs::last_write_time(d.path / "a.jpg", now - std::chrono::hours(3));
  fs::last_write_time(d.path / "b.jpg", now - std::chrono::hours(2));
  REQUIRE(c.get("a.jpg").has_value());  // a is now the newest
  REQUIRE(c.put("c.jpg", std::string(10, 'c')));
  CHECK(fs::exists(d.path / "a.jpg"));
  CHECK_FALSE(fs::exists(d.path / "b.jpg"));
}

TEST_CASE("put into an unwritable folder returns false and does not throw") {
  TempDir d;
  fs::create_directories(d.path);
  std::ofstream(d.path / "file") << "x";
  ImageCache c(d.path / "file" / "sub");  // a folder below a regular file
  CHECK_FALSE(c.put("a.jpg", "x"));
  CHECK_FALSE(c.get("a.jpg").has_value());
}

TEST_CASE("many puts keep the cache under the limit") {
  TempDir d;
  ImageCache c(d.path, 25);
  for (int i = 0; i < 6; ++i) REQUIRE(c.put("f" + std::to_string(i) + ".jpg", std::string(10, 'x')));
  size_t files = 0;
  for (const auto& e : fs::directory_iterator(d.path)) {
    if (e.path().extension() == ".jpg") ++files;
  }
  CHECK(files <= 2);
  CHECK(c.get("f5.jpg").has_value());
}

TEST_CASE("files from an earlier run count toward the limit") {
  TempDir d;
  {
    ImageCache first(d.path, 1000);
    REQUIRE(first.put("a.jpg", std::string(10, 'a')));
    REQUIRE(first.put("b.jpg", std::string(10, 'b')));
    REQUIRE(first.put("c.jpg", std::string(10, 'c')));
  }
  const auto now = fs::file_time_type::clock::now();
  fs::last_write_time(d.path / "a.jpg", now - std::chrono::hours(3));
  fs::last_write_time(d.path / "b.jpg", now - std::chrono::hours(2));
  ImageCache second(d.path, 20);
  REQUIRE(second.put("d.jpg", std::string(10, 'd')));  // 40 bytes in all: the two oldest go
  CHECK_FALSE(fs::exists(d.path / "a.jpg"));
  CHECK_FALSE(fs::exists(d.path / "b.jpg"));
  CHECK(fs::exists(d.path / "c.jpg"));
  CHECK(fs::exists(d.path / "d.jpg"));
}
