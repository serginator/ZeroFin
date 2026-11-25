#include "storage/image_cache.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <vector>

#include "storage/config_store.h"

namespace zerofin::storage {
namespace fs = std::filesystem;
namespace {

std::string safe(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) c = '_';
  }
  return out;
}

}  // namespace

ImageCache::ImageCache(fs::path dir, uint64_t max_bytes) : dir_(std::move(dir)), max_bytes_(max_bytes) {}

std::string ImageCache::file_name(const std::string& item_id, const std::string& tag, int w, int h) {
  return safe(item_id) + "-" + (tag.empty() ? "notag" : safe(tag)) + "-" + std::to_string(w) + "x" +
         std::to_string(h) + ".jpg";
}

std::optional<std::string> ImageCache::get(const std::string& file_name) {
  const fs::path path = dir_ / file_name;
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (bytes.empty()) return std::nullopt;
  std::error_code ec;
  fs::last_write_time(path, fs::file_time_type::clock::now(), ec);
  return bytes;
}

bool ImageCache::put(const std::string& file_name, const std::string& bytes) {
  std::error_code ec;
  fs::create_directories(dir_, ec);
  if (ec) return false;
  fs::permissions(dir_, fs::perms::owner_all, fs::perm_options::replace, ec);
  const fs::path tmp = dir_ / (file_name + ".tmp");
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();  // a full disk shows up here for a small file, not at the write
    if (out.fail()) {
      fs::remove(tmp, ec);
      return false;
    }
  }
  fs::permissions(tmp, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ec);
  fs::rename(tmp, dir_ / file_name, ec);
  if (ec) {
    fs::remove(tmp, ec);
    return false;
  }
  // The first put counts the files of an earlier run. After that the folder is read again only when the
  // running total passes the limit, so a put does not cost one stat per cached file.
  if (!scanned_ || total_ + bytes.size() > max_bytes_) evict(); else total_ += bytes.size();
  return true;
}

void ImageCache::evict() {
  struct Entry {
    fs::path path;
    uint64_t size;
    fs::file_time_type time;
  };
  std::vector<Entry> files;
  uint64_t total = 0;
  std::error_code ec;
  for (fs::directory_iterator it(dir_, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code e2;
    if (!it->is_regular_file(e2)) continue;
    const uint64_t size = it->file_size(e2);
    const auto time = it->last_write_time(e2);
    if (e2) continue;
    files.push_back({it->path(), size, time});
    total += size;
  }
  scanned_ = true;
  total_ = total;
  if (total <= max_bytes_) return;
  std::sort(files.begin(), files.end(), [](const Entry& a, const Entry& b) { return a.time < b.time; });
  for (const auto& f : files) {
    if (total <= max_bytes_) break;
    std::error_code e3;
    if (fs::remove(f.path, e3)) total -= f.size;
  }
  total_ = total;
}

fs::path default_image_cache_dir() { return default_config_dir() / "cache" / "images"; }

}  // namespace zerofin::storage
