#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace zerofin::storage {

// Poster files on disk. Every call runs on the image worker thread. A failure never throws:
// get() gives nullopt and put() gives false, and the caller goes on without the cache.
class ImageCache {
 public:
  explicit ImageCache(std::filesystem::path dir, uint64_t max_bytes = 50ull << 20);

  // "<id>-<tag>-<w>x<h>.jpg". A character outside [A-Za-z0-9_-] becomes '_'. An empty tag becomes "notag".
  static std::string file_name(const std::string& item_id, const std::string& tag, int w, int h);

  // The file content. Refreshes the file time, so eviction removes the files used longest ago first.
  std::optional<std::string> get(const std::string& file_name);
  // Writes a temp file and renames it (mode 0600), then calls evict().
  bool put(const std::string& file_name, const std::string& bytes);
  // Deletes the oldest files until the total size is at most max_bytes.
  void evict();

 private:
  std::filesystem::path dir_;
  uint64_t max_bytes_;
  bool scanned_ = false;   // evict() has run once, so total_ is known
  uint64_t total_ = 0;     // bytes in the folder after the last evict(), plus the puts since
};

// default_config_dir() / "cache" / "images"
std::filesystem::path default_image_cache_dir();

}  // namespace zerofin::storage
