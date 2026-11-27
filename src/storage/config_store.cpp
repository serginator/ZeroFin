#include "storage/config_store.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
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

std::string canonical_address(std::string url) {
  while (!url.empty() && url.back() == '/') url.pop_back();
  for (char& c : url) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return url;
}

}  // namespace

bool same_server_address(const std::string& a, const std::string& b) {
  const std::string ca = canonical_address(a);
  return !ca.empty() && ca == canonical_address(b);
}

void stash_active_server(AppConfig& c) {
  if (c.server_url.empty()) return;
  remove_saved_server(c, c.server_url);
  c.servers.insert(c.servers.begin(), SavedServer{c.server_url, c.server_name, c.token, c.user_id, c.user_name});
  c.server_url.clear();
  c.server_name.clear();
  c.token.clear();
  c.user_id.clear();
  c.user_name.clear();
}

bool activate_saved_server(AppConfig& c, const std::string& url) {
  const auto it = std::find_if(c.servers.begin(), c.servers.end(),
                               [&](const SavedServer& s) { return same_server_address(s.url, url); });
  if (it == c.servers.end()) return false;
  const SavedServer chosen = *it;
  c.servers.erase(it);
  stash_active_server(c);
  c.server_url = chosen.url;
  c.server_name = chosen.name;
  c.token = chosen.token;
  c.user_id = chosen.user_id;
  c.user_name = chosen.user_name;
  return true;
}

void select_server(AppConfig& c, const std::string& url, const std::string& name) {
  if (!same_server_address(c.server_url, url)) {
    if (!activate_saved_server(c, url)) {
      stash_active_server(c);
      c.server_url = url;
    }
  }
  if (!name.empty()) c.server_name = name;
}

bool remove_saved_server(AppConfig& c, const std::string& url) {
  const auto it = std::find_if(c.servers.begin(), c.servers.end(),
                               [&](const SavedServer& s) { return same_server_address(s.url, url); });
  if (it == c.servers.end()) return false;
  c.servers.erase(it);
  return true;
}

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
    c.video_output = (str(j, "video_output") == "lcd") ? "lcd" : "auto";
    c.hdmi_quality = (str(j, "hdmi_quality") == "1080") ? "1080" : "720";
    c.hdmi_sound = (str(j, "hdmi_sound") == "speaker") ? "speaker" : "monitor";
    if (auto list = j.find("servers"); list != j.end() && list->is_array()) {
      for (const json& entry : *list) {
        if (!entry.is_object()) continue;
        SavedServer s{str(entry, "url"), str(entry, "name"), str(entry, "token"), str(entry, "user_id"),
                      str(entry, "user_name")};
        if (!s.url.empty()) c.servers.push_back(std::move(s));
      }
    }
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

  json saved_servers = json::array();
  for (const SavedServer& s : c.servers) {
    saved_servers.push_back({{"url", s.url},
                             {"name", s.name},
                             {"token", s.token},
                             {"user_id", s.user_id},
                             {"user_name", s.user_name}});
  }
  const json j = {{"version", 1},
                  {"server_url", c.server_url},
                  {"server_name", c.server_name},
                  {"token", c.token},
                  {"user_id", c.user_id},
                  {"user_name", c.user_name},
                  {"device_id", c.device_id},
                  {"play_mode", c.play_mode == "transcode" ? "transcode" : "auto"},
                  {"video_output", c.video_output == "lcd" ? "lcd" : "auto"},
                  {"hdmi_quality", c.hdmi_quality == "1080" ? "1080" : "720"},
                  {"hdmi_sound", c.hdmi_sound == "speaker" ? "speaker" : "monitor"},
                  {"servers", saved_servers}};
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
