#include "storage/config_store.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace zerofin::storage {
namespace {

using nlohmann::json;
using jf::Error;
using jf::ErrorKind;

std::string str(const json& j, const char* key) {
  auto it = j.find(key);
  return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

Error io_error(const std::string& what) { return Error{ErrorKind::Invalid, what, 0}; }

}  // namespace

std::filesystem::path default_config_dir() {
  if (const char* home = std::getenv("ZEROFIN_HOME"); home && *home) return home;
  if (const char* home = std::getenv("HOME"); home && *home) return std::filesystem::path(home) / ".config" / "ZeroFin";
  return ".zerofin";
}

jf::Result<AppConfig> ConfigStore::load() const {
  std::error_code ec;
  if (!std::filesystem::exists(file_path(), ec)) return AppConfig{};

  std::ifstream in(file_path());
  if (!in) return io_error("cannot read " + file_path().string());
  std::stringstream buffer;
  buffer << in.rdbuf();

  try {
    const json j = json::parse(buffer.str());
    if (!j.is_object()) return Error{ErrorKind::Parse, "config is not a JSON object", 0};
    AppConfig c;
    c.server_url = str(j, "server_url");
    c.server_name = str(j, "server_name");
    c.token = str(j, "token");
    c.user_id = str(j, "user_id");
    c.user_name = str(j, "user_name");
    c.device_id = str(j, "device_id");
    const std::string mode = str(j, "play_mode");
    c.play_mode = (mode == "transcode") ? "transcode" : "auto";
    return c;
  } catch (const json::exception& e) {
    return Error{ErrorKind::Parse, e.what(), 0};
  }
}

jf::Result<jf::Unit> ConfigStore::save(const AppConfig& c) const {
  std::error_code ec;
  std::filesystem::create_directories(dir_, ec);
  if (ec) return io_error("cannot create " + dir_.string() + ": " + ec.message());
  ::chmod(dir_.c_str(), 0700);

  const json j = {{"version", 1},
                  {"server_url", c.server_url},
                  {"server_name", c.server_name},
                  {"token", c.token},
                  {"user_id", c.user_id},
                  {"user_name", c.user_name},
                  {"device_id", c.device_id},
                  {"play_mode", c.play_mode == "transcode" ? "transcode" : "auto"}};
  const std::string text = j.dump(2) + "\n";

  const std::filesystem::path tmp = dir_ / "config.json.tmp";
  const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) return io_error("cannot write " + tmp.string());
  const char* p = text.data();
  size_t left = text.size();
  while (left > 0) {
    const ssize_t n = ::write(fd, p, left);
    if (n <= 0) {
      ::close(fd);
      return io_error("write failed for " + tmp.string());
    }
    p += n;
    left -= static_cast<size_t>(n);
  }
  ::fsync(fd);
  ::close(fd);
  ::chmod(tmp.c_str(), 0600);

  std::filesystem::rename(tmp, file_path(), ec);
  if (ec) return io_error("cannot replace " + file_path().string() + ": " + ec.message());
  return jf::Unit{};
}

}  // namespace zerofin::storage
