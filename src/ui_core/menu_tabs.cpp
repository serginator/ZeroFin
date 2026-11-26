#include "ui_core/menu_tabs.h"

#include <algorithm>

namespace zerofin::ui {

std::vector<MenuTab> menu_tabs(bool playing_on_monitor, bool monitor_present) {
  std::vector<MenuTab> tabs = {MenuTab::Audio, MenuTab::Subtitles};
  if (playing_on_monitor) {
    tabs.push_back(MenuTab::Quality);
    tabs.push_back(MenuTab::Sound);
  }
  if (playing_on_monitor || monitor_present) tabs.push_back(MenuTab::Output);
  return tabs;
}

const char* menu_tab_label(MenuTab tab) {
  switch (tab) {
    case MenuTab::Audio: return "Audio";
    case MenuTab::Subtitles: return "Subs";
    case MenuTab::Quality: return "Quality";
    case MenuTab::Sound: return "Sound";
    case MenuTab::Output: return "Video";
  }
  return "";
}

MenuTab resolve_tab(const std::vector<MenuTab>& tabs, MenuTab wanted) {
  if (tabs.empty()) return wanted;
  return std::find(tabs.begin(), tabs.end(), wanted) != tabs.end() ? wanted : tabs.front();
}

MenuTab next_tab(const std::vector<MenuTab>& tabs, MenuTab current, int direction) {
  if (tabs.empty()) return current;
  const auto it = std::find(tabs.begin(), tabs.end(), current);
  if (it == tabs.end()) return tabs.front();
  const int count = static_cast<int>(tabs.size());
  const int index = (static_cast<int>(it - tabs.begin()) + direction % count + count) % count;
  return tabs[static_cast<size_t>(index)];
}

}  // namespace zerofin::ui
