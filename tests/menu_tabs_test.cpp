#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "ui_core/menu_tabs.h"

using namespace zerofin::ui;

TEST_CASE("tabs: the LCD with no monitor has Audio and Subs only") {
  const auto tabs = menu_tabs(false, false);
  REQUIRE(tabs.size() == 2);
  CHECK(tabs[0] == MenuTab::Audio);
  CHECK(tabs[1] == MenuTab::Subtitles);
}

TEST_CASE("tabs: playing on a monitor adds Quality, Sound and Video") {
  const auto tabs = menu_tabs(true, true);
  REQUIRE(tabs.size() == 5);
  CHECK(tabs[0] == MenuTab::Audio);
  CHECK(tabs[1] == MenuTab::Subtitles);
  CHECK(tabs[2] == MenuTab::Quality);
  CHECK(tabs[3] == MenuTab::Sound);
  CHECK(tabs[4] == MenuTab::Output);
}

TEST_CASE("tabs: a monitor is plugged in but the video is on the LCD, so only Video is added") {
  const auto tabs = menu_tabs(false, true);
  REQUIRE(tabs.size() == 3);
  CHECK(tabs[2] == MenuTab::Output);
}

TEST_CASE("tabs: the labels are short enough for a 320 px bar of five") {
  CHECK(std::string(menu_tab_label(MenuTab::Audio)) == "Audio");
  CHECK(std::string(menu_tab_label(MenuTab::Subtitles)) == "Subs");
  CHECK(std::string(menu_tab_label(MenuTab::Quality)) == "Quality");
  CHECK(std::string(menu_tab_label(MenuTab::Sound)) == "Sound");
  CHECK(std::string(menu_tab_label(MenuTab::Output)) == "Video");
}

TEST_CASE("tabs: moving wraps around at both ends") {
  const auto tabs = menu_tabs(true, true);
  CHECK(next_tab(tabs, MenuTab::Audio, 1) == MenuTab::Subtitles);
  CHECK(next_tab(tabs, MenuTab::Output, 1) == MenuTab::Audio);
  CHECK(next_tab(tabs, MenuTab::Audio, -1) == MenuTab::Output);
  CHECK(next_tab(tabs, MenuTab::Sound, -1) == MenuTab::Quality);
}

TEST_CASE("tabs: a tab that is not there (the monitor was unplugged) falls back to the first one") {
  const auto tabs = menu_tabs(false, false);
  CHECK(next_tab(tabs, MenuTab::Quality, 1) == MenuTab::Audio);
  CHECK(resolve_tab(tabs, MenuTab::Quality) == MenuTab::Audio);
  CHECK(resolve_tab(tabs, MenuTab::Subtitles) == MenuTab::Subtitles);
}

TEST_CASE("tabs: one tab cannot move") {
  const std::vector<MenuTab> one = {MenuTab::Audio};
  CHECK(next_tab(one, MenuTab::Audio, 1) == MenuTab::Audio);
  CHECK(next_tab(one, MenuTab::Audio, -1) == MenuTab::Audio);
}
