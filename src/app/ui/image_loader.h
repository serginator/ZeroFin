#pragma once

#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "jellyfin/client.h"
#include "jellyfin/http.h"
#include "jellyfin/models.h"
#include "lvgl.h"
#include "storage/image_cache.h"
#include "ui/async.h"
#include "ui_core/request_queue.h"

namespace zerofin::app {

// Downloads poster JPEGs one at a time on its own worker, keeps them on disk and in a small RAM cache,
// and hands LVGL image descriptors to the cards. UI thread only (the worker runs only the disk and network work).
class ImageLoader {
 public:
  static constexpr int kPosterW = 64;
  static constexpr int kPosterH = 96;

  // The JPEG bytes and an LVGL descriptor that points at them. The decoded bitmap lives in the LVGL image cache.
  struct Poster {
    std::string bytes;
    lv_image_dsc_t dsc{};
    ~Poster();
  };
  using PosterPtr = std::shared_ptr<const Poster>;

  struct Want {
    jf::ImageRef ref;
    int priority = 0;  // lower goes first
  };
  using Ready = std::function<void()>;

  ImageLoader(Async& worker, jf::HttpClient& http, storage::ImageCache cache);
  ~ImageLoader();
  ImageLoader(const ImageLoader&) = delete;
  ImageLoader& operator=(const ImageLoader&) = delete;

  // Replaces what is wanted. `on_ready` runs on the UI thread after a new poster arrives, while `alive` is true.
  void want(Async::Alive alive, std::vector<Want> wants, std::shared_ptr<jf::Client> client, Ready on_ready);
  // The poster from RAM, or null.
  PosterPtr get(const jf::ImageRef& ref);
  // No poster exists or it could not be fetched (until forget_failures()). An empty ref always fails.
  bool failed(const jf::ImageRef& ref) const;
  void forget_failures() { failed_.clear(); }

 private:
  struct Fetched {
    std::string key;
    std::string bytes;
    std::optional<jf::Error> error;
  };
  static std::string key_of(const jf::ImageRef& ref);
  void start_next();
  void finish(Fetched fetched);
  void store(const std::string& key, std::string bytes);

  Async& worker_;
  jf::HttpClient& http_;
  std::shared_ptr<storage::ImageCache> cache_;
  Async::Alive self_;   // false once this loader is gone
  Async::Alive owner_;  // the screen that asked last
  Ready on_ready_;
  ui::RequestQueue queue_;
  std::unordered_map<std::string, jf::HttpRequest> pending_;

  struct Cached {
    PosterPtr poster;
    std::list<std::string>::iterator pos;
  };
  std::unordered_map<std::string, Cached> ram_;
  std::list<std::string> lru_;  // most recently used first
  size_t ram_bytes_ = 0;
  std::unordered_set<std::string> failed_;
};

}  // namespace zerofin::app
