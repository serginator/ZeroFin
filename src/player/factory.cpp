#include "player/backend.h"
#include "player/demo_backend.h"

#if defined(ZEROFIN_HAS_MPV)
#include "player/mpv_backend.h"
#endif

namespace zerofin::player {

std::unique_ptr<PlayerBackend> make_backend(bool demo, const BackendOptions& options) {
  if (demo) return std::make_unique<DemoBackend>();
#if defined(ZEROFIN_HAS_MPV)
  return std::make_unique<MpvBackend>(options);
#else
  (void)options;
  return nullptr;
#endif
}

}  // namespace zerofin::player
