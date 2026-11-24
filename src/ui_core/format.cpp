#include "ui_core/format.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>

#include "jellyfin/playback_decision.h"
#include "jellyfin/resume.h"
#include "jellyfin/text.h"

namespace zerofin::ui {
namespace {

using jf::to_lower;
using jf::to_upper;

std::string channel_layout(int channels) {
  switch (channels) {
    case 1: return "1.0";
    case 2: return "2.0";
    case 3: return "2.1";
    case 6: return "5.1";
    case 8: return "7.1";
    default: return channels > 0 ? std::to_string(channels) + "ch" : "";
  }
}

std::string audio_codec_name(const std::string& codec) {
  const std::string c = to_lower(codec);
  if (c == "eac3") return "EAC3";
  if (c == "truehd") return "TrueHD";
  if (c == "dts") return "DTS";
  return to_upper(codec);
}

std::string subtitle_codec_name(const std::string& codec) {
  const std::string c = to_lower(codec);
  if (c == "subrip" || c == "srt") return "SRT";
  if (c == "ass" || c == "ssa") return "ASS";
  if (c == "webvtt" || c == "vtt") return "VTT";
  if (c == "pgssub" || c == "pgs" || c == "hdmv_pgs_subtitle") return "PGS";
  if (c == "dvdsub" || c == "dvd_subtitle") return "DVD";
  if (c == "dvbsub" || c == "dvb_subtitle") return "DVB";
  if (c == "mov_text") return "TXT";
  return to_upper(codec);
}

}  // namespace

std::string format_runtime(int64_t ticks) {
  if (ticks <= 0) return "";
  const long minutes = static_cast<long>(std::llround(jf::ticks_to_seconds(ticks) / 60.0));
  if (minutes < 60) return std::to_string(std::max(1L, minutes)) + "m";
  const long h = minutes / 60;
  const long m = minutes % 60;
  return m == 0 ? std::to_string(h) + "h" : std::to_string(h) + "h " + std::to_string(m) + "m";
}

std::string format_timecode(double seconds) {
  const long total = seconds > 0 ? static_cast<long>(seconds) : 0;
  const long h = total / 3600;
  const long m = (total % 3600) / 60;
  const long s = total % 60;
  char buf[32];
  if (h > 0) {
    std::snprintf(buf, sizeof(buf), "%ld:%02ld:%02ld", h, m, s);
  } else {
    std::snprintf(buf, sizeof(buf), "%ld:%02ld", m, s);
  }
  return buf;
}

std::string episode_label(const jf::Item& item) {
  if (item.index_number < 0) return "";
  if (item.parent_index_number < 0) return "E" + std::to_string(item.index_number);
  return "S" + std::to_string(item.parent_index_number) + "E" + std::to_string(item.index_number);
}

std::string format_bitrate(int64_t bps) {
  if (bps <= 0) return "";
  char buf[32];
  if (bps >= 1'000'000) {
    std::snprintf(buf, sizeof(buf), "%.1f Mbps", static_cast<double>(bps) / 1e6);
  } else {
    std::snprintf(buf, sizeof(buf), "%ld kbps", static_cast<long>(bps / 1000));
  }
  return buf;
}

std::string language_name(const std::string& code) {
  static const std::map<std::string, std::string> kNames = {
      {"eng", "English"},  {"spa", "Spanish"},    {"jpn", "Japanese"}, {"fra", "French"},    {"fre", "French"},
      {"deu", "German"},   {"ger", "German"},     {"ita", "Italian"},  {"por", "Portuguese"}, {"rus", "Russian"},
      {"kor", "Korean"},   {"zho", "Chinese"},    {"chi", "Chinese"},  {"cat", "Catalan"},   {"nld", "Dutch"},
      {"dut", "Dutch"},    {"pol", "Polish"},     {"tur", "Turkish"},  {"ara", "Arabic"},    {"hin", "Hindi"},
      {"swe", "Swedish"},  {"nor", "Norwegian"},  {"dan", "Danish"},   {"fin", "Finnish"},   {"ces", "Czech"},
      {"cze", "Czech"},    {"ell", "Greek"},      {"gre", "Greek"},    {"heb", "Hebrew"},    {"tha", "Thai"},
      {"vie", "Vietnamese"}, {"ukr", "Ukrainian"}, {"eus", "Basque"},  {"glg", "Galician"}};
  const std::string c = to_lower(code);
  if (c.empty() || c == "und") return "Unknown";
  auto it = kNames.find(c);
  return it != kNames.end() ? it->second : to_upper(code);
}

// Name for a track: its language, else its title, else "Track N" (container stream index).
static std::string stream_name(const jf::MediaStream& s) {
  const std::string lang = to_lower(s.language);
  if (!lang.empty() && lang != "und") return language_name(s.language);
  if (!s.title.empty()) return s.title;
  if (s.index >= 0) return "Track " + std::to_string(s.index);
  return language_name(s.language);
}

std::string audio_label(const jf::MediaStream& s) {
  std::string out = stream_name(s);
  std::string tech = audio_codec_name(s.codec);
  const std::string layout = channel_layout(s.channels);
  if (!layout.empty()) tech += (tech.empty() ? "" : " ") + layout;
  if (!tech.empty()) out += " · " + tech;
  return out;
}

std::string subtitle_label(const jf::MediaStream& s) {
  std::string out = stream_name(s);
  const std::string codec = subtitle_codec_name(s.codec);
  if (!codec.empty()) out += " · " + codec;
  // The same rule as the playback decision, so the label matches what play does.
  const bool image = jf::is_image_subtitle(s);
  if (image) out += " (burned in)";
  if (s.is_forced) out += " forced";
  return out;
}

std::string truncate_text(const std::string& text, size_t max_chars) {
  size_t chars = 0;
  size_t i = 0;
  size_t cut = text.size();
  while (i < text.size()) {
    if (chars == max_chars) {
      cut = i;
      break;
    }
    const unsigned char c = static_cast<unsigned char>(text[i]);
    i += (c < 0x80) ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
    ++chars;
  }
  if (cut >= text.size()) return text;
  return text.substr(0, cut) + "...";
}

RowText describe_item(const jf::Item& item, RowContext context) {
  RowText row;
  row.watched = item.user_data.played;
  const double fraction = jf::progress_fraction(item.user_data.position_ticks, item.runtime_ticks);
  if (fraction > 0 && !item.user_data.played) row.progress = fraction;

  if (context == RowContext::Continue) {
    if (item.type == jf::ItemType::Episode) {
      row.primary = item.series_name.empty() ? item.name : item.series_name;
      row.detail = episode_label(item);
    } else {
      row.primary = item.name;
      row.detail = item.production_year > 0 ? std::to_string(item.production_year) : "";
    }
    return row;
  }

  switch (item.type) {
    case jf::ItemType::Episode:
      row.primary = (item.index_number >= 0 ? std::to_string(item.index_number) + ". " : "") + item.name;
      row.detail = format_runtime(item.runtime_ticks);
      break;
    case jf::ItemType::Movie:
    case jf::ItemType::Series:
      row.primary = item.name;
      row.detail = item.production_year > 0 ? std::to_string(item.production_year) : "";
      break;
    default:
      row.primary = item.name;
      break;
  }
  return row;
}

}  // namespace zerofin::ui
