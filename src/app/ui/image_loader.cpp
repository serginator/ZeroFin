#include "ui/image_loader.h"

#include "jellyfin/image_fetch.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "src/misc/cache/instance/lv_image_header_cache.h"

namespace zerofin::app {
namespace {
constexpr size_t kRamLimit = 3 * 1024 * 1024;
}

ImageLoader::Poster::~Poster() {
  // LVGL keys its decoded-image cache by the address of the descriptor. A new Poster can get the same address.
  lv_image_cache_drop(&dsc);
  lv_image_header_cache_drop(&dsc);
}

ImageLoader::ImageLoader(Async& worker, jf::HttpClient& http, storage::ImageCache cache)
    : worker_(worker), http_(http), cache_(std::make_shared<storage::ImageCache>(std::move(cache))),
      self_(Async::make_alive()) {}

ImageLoader::~ImageLoader() { *self_ = false; }

std::string ImageLoader::key_of(const jf::ImageRef& ref) {
  return storage::ImageCache::file_name(ref.item_id, ref.tag, kPosterW, kPosterH);
}

bool ImageLoader::failed(const jf::ImageRef& ref) const { return ref.empty() || failed_.count(key_of(ref)) > 0; }

ImageLoader::PosterPtr ImageLoader::get(const jf::ImageRef& ref) {
  if (ref.empty()) return nullptr;
  auto it = ram_.find(key_of(ref));
  if (it == ram_.end()) return nullptr;
  lru_.splice(lru_.begin(), lru_, it->second.pos);
  return it->second.poster;
}

void ImageLoader::want(Async::Alive alive, std::vector<Want> wants, std::shared_ptr<jf::Client> client, Ready on_ready) {
  owner_ = std::move(alive);
  on_ready_ = std::move(on_ready);
  pending_.clear();
  std::vector<ui::RequestQueue::Entry> entries;
  for (const auto& w : wants) {
    if (w.ref.empty()) continue;
    const std::string key = key_of(w.ref);
    if (ram_.count(key) || failed_.count(key) || pending_.count(key)) continue;
    pending_[key] = client->image_request(w.ref, kPosterW, kPosterH);
    entries.push_back({key, w.priority});
  }
  queue_.set_wanted(std::move(entries));
  start_next();
}

void ImageLoader::start_next() {
  const auto key = queue_.next();
  if (!key) return;
  auto it = pending_.find(*key);  // every key the queue gives out was put in pending_ by the same want()
  jf::HttpRequest request = std::move(it->second);
  pending_.erase(it);
  auto cache = cache_;
  jf::HttpClient* http = &http_;
  worker_.run(
      self_,
      [cache, http, request = std::move(request), k = *key] {
        Fetched f;
        f.key = k;
        if (auto bytes = cache->get(k); bytes && jf::is_baseline_jpeg(*bytes)) {
          f.bytes = std::move(*bytes);
          return f;
        }
        auto r = jf::fetch_image(*http, request);
        if (r) {
          cache->put(k, r.value());
          f.bytes = std::move(r.value());
        } else {
          f.error = r.error();
        }
        return f;
      },
      [this](Fetched f) { finish(std::move(f)); });
}

void ImageLoader::finish(Fetched f) {
  queue_.done(f.key);
  const bool good = !f.error.has_value();
  if (good) {
    store(f.key, std::move(f.bytes));
  } else {
    // A missing image (404), a refused one (401: the main worker handles the login) and a body that is
    // not a baseline JPEG will not change. A timeout or a 5xx may work on the next request.
    const bool permanent = f.error->http_status == 401 || f.error->http_status == 404 ||
                           f.error->kind == jf::ErrorKind::Parse;
    if (permanent) failed_.insert(f.key);
  }
  start_next();
  if (good && owner_ && *owner_ && on_ready_) {
    const Ready ready = on_ready_;  // the callback may call want(), which replaces on_ready_
    ready();
  }
}

void ImageLoader::store(const std::string& key, std::string bytes) {
  const auto size = jf::jpeg_size(bytes);
  if (!size) {
    failed_.insert(key);
    return;
  }
  auto poster = std::make_shared<Poster>();
  poster->bytes = std::move(bytes);
  poster->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  poster->dsc.header.cf = LV_COLOR_FORMAT_RAW;
  poster->dsc.header.w = static_cast<uint32_t>(size->first);
  poster->dsc.header.h = static_cast<uint32_t>(size->second);
  poster->dsc.data = reinterpret_cast<const uint8_t*>(poster->bytes.data());
  poster->dsc.data_size = static_cast<uint32_t>(poster->bytes.size());

  ram_bytes_ +=poster->bytes.size();
  lru_.push_front(key);
  ram_[key] = Cached{std::move(poster), lru_.begin()};
  // The cards that show an evicted poster still hold it (shared_ptr), so eviction never frees a poster in use.
  while (ram_bytes_ > kRamLimit && lru_.size() > 1) {
    const std::string& oldest = lru_.back();
    auto it = ram_.find(oldest);
    ram_bytes_ -= it->second.poster->bytes.size();
    ram_.erase(it);
    lru_.pop_back();
  }
}

}  // namespace zerofin::app
