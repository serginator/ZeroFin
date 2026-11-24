// ZeroFin playback spike.
//
// Plays one URL with libmpv (software render API) into an LVGL canvas shown on
// the Linux framebuffer. Logs one CSV line per second, then a summary, so the
// spec's pass bar can be checked (>= 20 fps for 10 minutes, A/V drift < 100 ms,
// no crash from CMA or memory pressure).
//
// Usage: zerofin_spike [--duration S] [--fmt rgb565|rgb0] [--fb PATH]
//                      [--opt key=value]... [--no-display] URL

#include <lvgl.h>
#include <mpv/client.h>
#include <mpv/render.h>

#include <poll.h>
#include <sys/eventfd.h>
#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 170;
constexpr int kWarmupSeconds = 5;  // ignored for min fps and the drift baseline
constexpr double kPassFps = 20.0;
constexpr double kPassDriftSeconds = 0.100;

std::atomic<bool> g_stop{false};
int g_wake_fd = -1;

void on_signal(int) {
  g_stop = true;
  if (g_wake_fd >= 0) {
    uint64_t one = 1;
    (void)!write(g_wake_fd, &one, sizeof(one));
  }
}

void wake(void*) {
  uint64_t one = 1;
  (void)!write(g_wake_fd, &one, sizeof(one));
}

uint32_t tick_ms() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint32_t>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

double now_s() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

double cpu_seconds() {
  rusage ru{};
  getrusage(RUSAGE_SELF, &ru);
  return static_cast<double>(ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) +
         static_cast<double>(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1e6;
}

double rss_mb() {
  std::ifstream f("/proc/self/statm");
  long pages_total = 0, pages_rss = 0;
  f >> pages_total >> pages_rss;
  return static_cast<double>(pages_rss) * static_cast<double>(sysconf(_SC_PAGESIZE)) / (1024.0 * 1024.0);
}

long meminfo_kb(const char* key) {
  std::ifstream f("/proc/meminfo");
  std::string name;
  long value = 0;
  std::string unit;
  while (f >> name >> value >> unit) {
    if (name == std::string(key) + ":") return value;
  }
  return -1;
}

double temp_c() {
  std::ifstream f("/sys/class/thermal/thermal_zone0/temp");
  long milli = 0;
  f >> milli;
  return static_cast<double>(milli) / 1000.0;
}

int64_t prop_int(mpv_handle* mpv, const char* name, int64_t fallback) {
  int64_t v = 0;
  return mpv_get_property(mpv, name, MPV_FORMAT_INT64, &v) >= 0 ? v : fallback;
}

std::string prop_string(mpv_handle* mpv, const char* name) {
  char* s = mpv_get_property_string(mpv, name);
  std::string out = s ? s : "";
  if (s) mpv_free(s);
  return out;
}

struct Options {
  std::string url;
  std::string fb = "/dev/fb0";
  std::string fmt = "rgb565";
  std::string log_level = "warn";
  double duration = 0;  // 0 = until the file ends
  bool no_display = false;
  std::vector<std::pair<std::string, std::string>> mpv_opts;
};

bool parse_args(int argc, char** argv, Options& o) {
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
    if (a == "--duration") {
      const char* v = next();
      if (!v) return false;
      o.duration = std::atof(v);
    } else if (a == "--fmt") {
      const char* v = next();
      if (!v) return false;
      o.fmt = v;
    } else if (a == "--log") {
      const char* v = next();
      if (!v) return false;
      o.log_level = v;
    } else if (a == "--fb") {
      const char* v = next();
      if (!v) return false;
      o.fb = v;
    } else if (a == "--opt") {
      const char* v = next();
      if (!v) return false;
      std::string kv = v;
      auto eq = kv.find('=');
      if (eq == std::string::npos) return false;
      o.mpv_opts.emplace_back(kv.substr(0, eq), kv.substr(eq + 1));
    } else if (a == "--no-display") {
      o.no_display = true;
    } else if (!a.empty() && a[0] != '-') {
      o.url = a;
    } else {
      return false;
    }
  }
  return !o.url.empty();
}

std::atomic<uint64_t> g_lvgl_refreshes{0};

void on_refr_finish(lv_event_t*) { ++g_lvgl_refreshes; }

struct Sample {
  double t;
  double mpv_fps;
  double lvgl_fps;
  double cores;
  double avsync;
  bool has_avsync;
};

}  // namespace

int main(int argc, char** argv) {
  Options opt;
  if (!parse_args(argc, argv, opt)) {
    std::fprintf(stderr,
                 "usage: %s [--duration S] [--fmt rgb565|rgb0] [--fb PATH] [--log LEVEL] [--opt k=v]... "
                 "[--no-display] URL\n",
                 argv[0]);
    return 2;
  }

  g_wake_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  // --- LVGL: framebuffer display with one full-screen canvas ---
  lv_init();
  lv_tick_set_cb(tick_ms);
  lv_display_t* disp = lv_linux_fbdev_create();
  if (!disp || lv_linux_fbdev_set_file(disp, opt.fb.c_str()) != LV_RESULT_OK) {
    std::fprintf(stderr, "cannot open framebuffer %s\n", opt.fb.c_str());
    return 1;
  }
  std::fprintf(stderr, "display %dx%d\n", static_cast<int>(lv_display_get_horizontal_resolution(disp)),
               static_cast<int>(lv_display_get_vertical_resolution(disp)));
  lv_display_add_event_cb(disp, on_refr_finish, LV_EVENT_REFR_READY, nullptr);

  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  lv_draw_buf_t* draw_buf = lv_draw_buf_create(kWidth, kHeight, LV_COLOR_FORMAT_RGB565, 0);
  if (!draw_buf) {
    std::fprintf(stderr, "cannot allocate canvas buffer\n");
    return 1;
  }
  lv_obj_t* canvas = lv_canvas_create(screen);
  lv_canvas_set_draw_buf(canvas, draw_buf);
  lv_obj_set_pos(canvas, 0, 0);
  lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);

  // --- mpv ---
  mpv_handle* mpv = mpv_create();
  if (!mpv) return 1;
  mpv_set_option_string(mpv, "vo", "libmpv");
  mpv_set_option_string(mpv, "config", "no");
  mpv_set_option_string(mpv, "terminal", "no");
  mpv_set_option_string(mpv, "load-scripts", "no");
  mpv_set_option_string(mpv, "ytdl", "no");
  mpv_set_option_string(mpv, "input-default-bindings", "no");
  mpv_set_option_string(mpv, "demuxer-max-bytes", "16MiB");
  mpv_set_option_string(mpv, "demuxer-max-back-bytes", "1MiB");
  for (const auto& [k, v] : opt.mpv_opts) {
    int rc = mpv_set_option_string(mpv, k.c_str(), v.c_str());
    if (rc < 0) std::fprintf(stderr, "mpv option %s=%s rejected: %s\n", k.c_str(), v.c_str(), mpv_error_string(rc));
  }
  if (mpv_initialize(mpv) < 0) {
    std::fprintf(stderr, "mpv_initialize failed\n");
    return 1;
  }
  mpv_request_log_messages(mpv, opt.log_level.c_str());
  mpv_set_wakeup_callback(mpv, wake, nullptr);

  mpv_render_param ctx_params[] = {
      {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_SW)},
      {MPV_RENDER_PARAM_INVALID, nullptr},
  };
  mpv_render_context* rctx = nullptr;
  if (mpv_render_context_create(&rctx, mpv, ctx_params) < 0) {
    std::fprintf(stderr, "mpv_render_context_create failed\n");
    return 1;
  }
  mpv_render_context_set_update_callback(rctx, wake, nullptr);

  const char* cmd[] = {"loadfile", opt.url.c_str(), nullptr};
  const double t_load = now_s();
  mpv_command(mpv, cmd);

  std::printf(
      "t,mpv_fps,lvgl_fps,cores,avsync_ms,drops,dec_drops,delayed,rss_mb,cma_free_kb,temp_c,hwdec,pos_s,cache_s,net_kBps,"
      "paused_for_cache\n");
  std::fflush(stdout);

  // --- main loop ---
  uint64_t frames = 0, frames_prev = 0, lvgl_prev = 0;
  double first_frame_s = -1;
  double last_tick = now_s(), cpu_prev = cpu_seconds();
  const double t_start = last_tick;
  std::vector<Sample> samples;
  double rss_peak = 0;
  long cma_min = -1;
  double temp_max = 0;
  int64_t drops = 0, dec_drops = 0, delayed = 0;
  std::string hwdec_used;
  bool ended = false;
  std::string end_reason;
  bool render_format_logged = false;

  while (!g_stop && !ended) {
    const uint32_t lv_wait = lv_timer_handler();
    const int timeout = static_cast<int>(std::min<uint32_t>(lv_wait, 8));
    pollfd pfd{g_wake_fd, POLLIN, 0, };
    poll(&pfd, 1, timeout);
    uint64_t drain = 0;
    (void)!read(g_wake_fd, &drain, sizeof(drain));

    while (true) {
      mpv_event* ev = mpv_wait_event(mpv, 0);
      if (ev->event_id == MPV_EVENT_NONE) break;
      if (ev->event_id == MPV_EVENT_LOG_MESSAGE) {
        auto* m = static_cast<mpv_event_log_message*>(ev->data);
        std::fprintf(stderr, "[mpv/%s] %s: %s", m->level, m->prefix, m->text);
      } else if (ev->event_id == MPV_EVENT_END_FILE) {
        auto* e = static_cast<mpv_event_end_file*>(ev->data);
        ended = true;
        end_reason = e->reason == MPV_END_FILE_REASON_EOF ? "eof"
                     : e->error < 0                       ? std::string("error: ") + mpv_error_string(e->error)
                                                          : "stopped";
      } else if (ev->event_id == MPV_EVENT_SHUTDOWN) {
        ended = true;
        end_reason = "shutdown";
      }
    }

    if (mpv_render_context_update(rctx) & MPV_RENDER_UPDATE_FRAME) {
      int size[2] = {kWidth, kHeight};
      size_t stride = draw_buf->header.stride;
      mpv_render_param rp[] = {
          {MPV_RENDER_PARAM_SW_SIZE, size},
          {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char*>(opt.fmt.c_str())},
          {MPV_RENDER_PARAM_SW_STRIDE, &stride},
          {MPV_RENDER_PARAM_SW_POINTER, draw_buf->data},
          {MPV_RENDER_PARAM_INVALID, nullptr},
      };
      int rc = mpv_render_context_render(rctx, rp);
      if (rc < 0) {
        std::fprintf(stderr, "render failed: %s\n", mpv_error_string(rc));
        ended = true;
        end_reason = "render error";
      } else {
        if (!render_format_logged) {
          std::fprintf(stderr, "first frame after %.2fs\n", now_s() - t_load);
          render_format_logged = true;
        }
        ++frames;
        if (first_frame_s < 0) first_frame_s = now_s() - t_load;
        if (!opt.no_display) lv_obj_invalidate(canvas);
      }
    }

    const double now = now_s();
    if (now - last_tick >= 1.0) {
      const double dt = now - last_tick;
      const double cpu = cpu_seconds();
      Sample s{};
      s.t = now - t_start;
      s.mpv_fps = static_cast<double>(frames - frames_prev) / dt;
      s.lvgl_fps = static_cast<double>(g_lvgl_refreshes.load() - lvgl_prev) / dt;
      s.cores = (cpu - cpu_prev) / dt;
      double avsync = 0;
      s.has_avsync = mpv_get_property(mpv, "avsync", MPV_FORMAT_DOUBLE, &avsync) >= 0;
      s.avsync = avsync;
      drops = prop_int(mpv, "frame-drop-count", drops);
      dec_drops = prop_int(mpv, "decoder-frame-drop-count", dec_drops);
      delayed = prop_int(mpv, "vo-delayed-frame-count", delayed);
      hwdec_used = prop_string(mpv, "hwdec-current");
      const double rss = rss_mb();
      const long cma = meminfo_kb("CmaFree");
      const double temp = temp_c();
      rss_peak = std::max(rss_peak, rss);
      if (cma >= 0 && (cma_min < 0 || cma < cma_min)) cma_min = cma;
      temp_max = std::max(temp_max, temp);
      samples.push_back(s);
      double pos = 0, cache_s = 0;
      if (mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos) < 0) pos = NAN;
      if (mpv_get_property(mpv, "demuxer-cache-duration", MPV_FORMAT_DOUBLE, &cache_s) < 0) cache_s = NAN;
      const int64_t net_bps = prop_int(mpv, "cache-speed", -1);
      const std::string pfc = prop_string(mpv, "paused-for-cache");
      std::printf("%.0f,%.1f,%.1f,%.2f,%.1f,%lld,%lld,%lld,%.1f,%ld,%.1f,%s,%.1f,%.1f,%lld,%s\n", s.t, s.mpv_fps,
                  s.lvgl_fps, s.cores, s.has_avsync ? s.avsync * 1000.0 : NAN, static_cast<long long>(drops),
                  static_cast<long long>(dec_drops), static_cast<long long>(delayed), rss, cma, temp,
                  hwdec_used.empty() ? "-" : hwdec_used.c_str(), pos, cache_s, static_cast<long long>(net_bps / 1000),
                  pfc.empty() ? "-" : pfc.c_str());
      std::fflush(stdout);
      frames_prev = frames;
      lvgl_prev = g_lvgl_refreshes.load();
      cpu_prev = cpu;
      last_tick = now;
    }
    if (opt.duration > 0 && now - t_start >= opt.duration) {
      end_reason = "duration reached";
      break;
    }
  }
  if (end_reason.empty()) end_reason = g_stop ? "signal" : "unknown";

  // --- summary ---
  const double total = now_s() - t_start;
  std::vector<double> steady_fps;
  double drift_first = NAN, drift_last = NAN, drift_max = 0, cores_sum = 0;
  int cores_n = 0;
  for (const auto& s : samples) {
    if (s.t <= kWarmupSeconds) continue;
    steady_fps.push_back(s.mpv_fps);
    cores_sum += s.cores;
    ++cores_n;
    if (s.has_avsync) {
      if (std::isnan(drift_first)) drift_first = s.avsync;
      drift_last = s.avsync;
      drift_max = std::max(drift_max, std::fabs(s.avsync));
    }
  }
  double fps_min = 0, fps_avg = 0;
  int below = 0;
  if (!steady_fps.empty()) {
    fps_min = *std::min_element(steady_fps.begin(), steady_fps.end());
    for (double f : steady_fps) {
      fps_avg += f;
      if (f < kPassFps) ++below;
    }
    fps_avg /= static_cast<double>(steady_fps.size());
  }
  const double drift_change = std::isnan(drift_first) ? NAN : std::fabs(drift_last - drift_first);
  std::fprintf(stderr,
               "\n==== summary ====\n"
               "end reason        : %s\n"
               "run time          : %.0f s (%zu steady 1 s windows after %d s warm-up)\n"
               "first frame       : %.2f s after loadfile\n"
               "mpv fps           : avg %.1f, min %.1f, windows below %.0f fps: %d\n"
               "avg CPU           : %.2f cores\n"
               "A/V sync          : first %.1f ms, last %.1f ms, change %.1f ms, max abs %.1f ms\n"
               "frame drops       : vo %lld, decoder %lld, delayed %lld\n"
               "RSS peak          : %.1f MB\n"
               "CMA free min      : %ld kB\n"
               "temperature max   : %.1f C\n"
               "hwdec in use      : %s\n"
               "pass bar          : fps>=%.0f: %s, drift<%.0f ms: %s\n",
               end_reason.c_str(), total, steady_fps.size(), kWarmupSeconds, first_frame_s, fps_avg, fps_min, kPassFps,
               below, cores_n ? cores_sum / cores_n : 0.0, drift_first * 1000, drift_last * 1000, drift_change * 1000,
               drift_max * 1000, static_cast<long long>(drops), static_cast<long long>(dec_drops),
               static_cast<long long>(delayed), rss_peak, cma_min, temp_max, hwdec_used.empty() ? "-" : hwdec_used.c_str(),
               kPassFps, (!steady_fps.empty() && below == 0) ? "PASS" : "FAIL", kPassDriftSeconds * 1000,
               (!std::isnan(drift_change) && drift_max < kPassDriftSeconds) ? "PASS" : "FAIL/unknown");

  mpv_render_context_free(rctx);
  mpv_terminate_destroy(mpv);
  lv_obj_delete(canvas);
  lv_draw_buf_destroy(draw_buf);
  lv_display_delete(disp);
  lv_deinit();
  return 0;
}
