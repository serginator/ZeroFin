#include <doctest/doctest.h>

#include "ui_core/format.h"

using namespace zerofin;
using namespace zerofin::ui;

TEST_CASE("format_runtime") {
  CHECK(format_runtime(0) == "");
  CHECK(format_runtime(jf::seconds_to_ticks(45 * 60)) == "45m");
  CHECK(format_runtime(jf::seconds_to_ticks(112 * 60)) == "1h 52m");
  CHECK(format_runtime(jf::seconds_to_ticks(120 * 60)) == "2h");
  CHECK(format_runtime(jf::seconds_to_ticks(20)) == "1m");
}

TEST_CASE("format_timecode") {
  CHECK(format_timecode(7) == "0:07");
  CHECK(format_timecode(754) == "12:34");
  CHECK(format_timecode(3723) == "1:02:03");
  CHECK(format_timecode(-5) == "0:00");
  CHECK(format_timecode(59.9) == "0:59");
}

TEST_CASE("episode_label") {
  jf::Item ep;
  ep.index_number = 5;
  ep.parent_index_number = 2;
  CHECK(episode_label(ep) == "S2E5");
  ep.parent_index_number = -1;
  CHECK(episode_label(ep) == "E5");
  ep.index_number = -1;
  CHECK(episode_label(ep) == "");
}

TEST_CASE("format_bitrate") {
  CHECK(format_bitrate(5'200'000) == "5.2 Mbps");
  CHECK(format_bitrate(850'000) == "850 kbps");
  CHECK(format_bitrate(0) == "");
}

TEST_CASE("language_name") {
  CHECK(language_name("eng") == "English");
  CHECK(language_name("ENG") == "English");
  CHECK(language_name("spa") == "Spanish");
  CHECK(language_name("xyz") == "XYZ");
  CHECK(language_name("") == "Unknown");
  CHECK(language_name("und") == "Unknown");
}

TEST_CASE("audio_label") {
  jf::MediaStream s;
  s.language = "jpn";
  s.codec = "aac";
  s.channels = 2;
  CHECK(audio_label(s) == "Japanese · AAC 2.0");
  s.language = "eng";
  s.codec = "eac3";
  s.channels = 6;
  CHECK(audio_label(s) == "English · EAC3 5.1");
  jf::MediaStream bare;
  bare.codec = "mp3";
  CHECK(audio_label(bare) == "Unknown · MP3");
  bare.index = 2;
  CHECK(audio_label(bare) == "Track 2 · MP3");
  bare.title = "Commentary";
  CHECK(audio_label(bare) == "Commentary · MP3");
  bare.language = "und";
  CHECK(audio_label(bare) == "Commentary · MP3");
}

TEST_CASE("subtitle_label marks image subtitles and forced tracks") {
  jf::MediaStream text;
  text.language = "eng";
  text.codec = "subrip";
  text.type = jf::StreamType::Subtitle;
  text.is_text_subtitle = true;
  CHECK(subtitle_label(text) == "English · SRT");
  text.is_forced = true;
  CHECK(subtitle_label(text) == "English · SRT forced");

  jf::MediaStream pgs;
  pgs.language = "spa";
  pgs.codec = "PGSSUB";
  pgs.type = jf::StreamType::Subtitle;
  CHECK(subtitle_label(pgs) == "Spanish · PGS (burned in)");

  // A codec the list does not know, and that is not text, is burned in by the playback decision too.
  jf::MediaStream odd;
  odd.language = "eng";
  odd.codec = "xsub";
  odd.type = jf::StreamType::Subtitle;
  CHECK(subtitle_label(odd).find("(burned in)") != std::string::npos);
}

TEST_CASE("truncate_text counts characters, not bytes") {
  CHECK(truncate_text("hello", 10) == "hello");
  CHECK(truncate_text("hello", 5) == "hello");
  CHECK(truncate_text("hello world", 5) == "hello...");
  CHECK(truncate_text("Pel\xC3\xAD" "culas de acci\xC3\xB3n", 5) == "Pel\xC3\xAD" "c...");
  CHECK(truncate_text("", 3) == "");
}

TEST_CASE("describe_item for the Continue rows") {
  jf::Item ep;
  ep.type = jf::ItemType::Episode;
  ep.name = "Pilot";
  ep.series_name = "Dragon Ball";
  ep.index_number = 1;
  ep.parent_index_number = 2;
  ep.runtime_ticks = jf::seconds_to_ticks(1000);
  ep.user_data.position_ticks = jf::seconds_to_ticks(250);
  const RowText row = describe_item(ep, RowContext::Continue);
  CHECK(row.primary == "Dragon Ball");
  CHECK(row.detail == "S2E1");
  CHECK(row.progress == doctest::Approx(0.25));

  jf::Item movie;
  movie.type = jf::ItemType::Movie;
  movie.name = "Heat";
  movie.production_year = 1995;
  const RowText m = describe_item(movie, RowContext::Continue);
  CHECK(m.primary == "Heat");
  CHECK(m.detail == "1995");
  CHECK(m.progress < 0);
}

TEST_CASE("describe_item for library lists") {
  jf::Item ep;
  ep.type = jf::ItemType::Episode;
  ep.name = "Pilot";
  ep.index_number = 5;
  ep.runtime_ticks = jf::seconds_to_ticks(45 * 60);
  const RowText row = describe_item(ep, RowContext::Browse);
  CHECK(row.primary == "5. Pilot");
  CHECK(row.detail == "45m");

  jf::Item series;
  series.type = jf::ItemType::Series;
  series.name = "Fleabag";
  series.production_year = 2016;
  CHECK(describe_item(series, RowContext::Browse).detail == "2016");

  jf::Item season;
  season.type = jf::ItemType::Season;
  season.name = "Season 2";
  const RowText s = describe_item(season, RowContext::Browse);
  CHECK(s.primary == "Season 2");
  CHECK(s.detail.empty());
}

TEST_CASE("a watched item shows no progress bar") {
  jf::Item movie;
  movie.type = jf::ItemType::Movie;
  movie.runtime_ticks = jf::seconds_to_ticks(1000);
  movie.user_data.position_ticks = jf::seconds_to_ticks(900);
  movie.user_data.played = true;
  const RowText row = describe_item(movie, RowContext::Browse);
  CHECK(row.watched);
  CHECK(row.progress < 0);
}
