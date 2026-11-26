#pragma once

#include <vector>

namespace zerofin::ui {

// The tabs of the player menu (the gear icon, or the Tab key).
enum class MenuTab { Audio, Subtitles, Quality, Sound, Output };

// The tabs to show, in order. Quality and Sound only matter while the video plays on a monitor.
// Video (the output choice) is there whenever a monitor is plugged in, so the user can move the video back.
std::vector<MenuTab> menu_tabs(bool playing_on_monitor, bool monitor_present);

// A short label, so that five tabs fit in 320 px.
const char* menu_tab_label(MenuTab tab);

// The tab to show when `wanted` may not exist any more: `wanted`, else the first tab.
MenuTab resolve_tab(const std::vector<MenuTab>& tabs, MenuTab wanted);

// The next tab (direction +1) or the previous one (-1). Wraps around.
MenuTab next_tab(const std::vector<MenuTab>& tabs, MenuTab current, int direction);

}  // namespace zerofin::ui
