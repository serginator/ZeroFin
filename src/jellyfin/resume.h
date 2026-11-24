#pragma once

#include <cstdint>

#include "jellyfin/models.h"

namespace zerofin::jf {

struct ResumeRules {
  double min_resume_seconds = 30;    // positions before this start from the beginning
  double max_resume_fraction = 0.95; // positions after this count as finished
  double rewind_seconds = 5;         // step back so the viewer sees the last words again
  double watched_fraction = 0.90;    // the server marks an item watched past this point
};

// Where to start playback, in ticks. Returns 0 to start from the beginning.
int64_t resume_start_ticks(const UserData& data, int64_t runtime_ticks, const ResumeRules& rules = {});

// True when `position_ticks` is far enough that the item counts as watched.
bool is_watched_position(int64_t position_ticks, int64_t runtime_ticks, const ResumeRules& rules = {});

// Progress in the range 0.0 - 1.0, for the progress bar on a card. 0 when unknown.
double progress_fraction(int64_t position_ticks, int64_t runtime_ticks);

}  // namespace zerofin::jf
