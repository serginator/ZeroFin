#include "jellyfin/resume.h"

#include <algorithm>

namespace zerofin::jf {

double progress_fraction(int64_t position_ticks, int64_t runtime_ticks) {
  if (runtime_ticks <= 0 || position_ticks <= 0) return 0.0;
  return std::clamp(static_cast<double>(position_ticks) / static_cast<double>(runtime_ticks), 0.0, 1.0);
}

bool is_watched_position(int64_t position_ticks, int64_t runtime_ticks, const ResumeRules& rules) {
  return runtime_ticks > 0 && progress_fraction(position_ticks, runtime_ticks) >= rules.watched_fraction;
}

int64_t resume_start_ticks(const UserData& data, int64_t runtime_ticks, const ResumeRules& rules) {
  if (data.played) return 0;
  if (data.position_ticks < seconds_to_ticks(rules.min_resume_seconds)) return 0;
  if (runtime_ticks > 0 && progress_fraction(data.position_ticks, runtime_ticks) > rules.max_resume_fraction) {
    return 0;
  }
  return std::max<int64_t>(0, data.position_ticks - seconds_to_ticks(rules.rewind_seconds));
}

}  // namespace zerofin::jf
