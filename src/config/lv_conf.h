/* LVGL 9.5 configuration for ZeroFin. One file for the SDL simulator and the device.
 * Derived from the CardputerZero Template config (MIT, M5Stack CardputerZero Community). */
#ifndef LV_CONF_H
#define LV_CONF_H

#ifndef USE_DESKTOP
#define USE_DESKTOP 0
#endif

/* The panel is RGB565. The simulator uses the same depth so pictures match. */
#define LV_COLOR_DEPTH 16

#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

/* LVGL runs on the UI thread only. Network work uses our own worker thread. */
#define LV_USE_OS LV_OS_NONE

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#if !USE_DESKTOP
#define LV_USE_LINUX_FBDEV 1
#define LV_LINUX_FBDEV_RENDER_MODE LV_DISPLAY_RENDER_MODE_PARTIAL
#define LV_LINUX_FBDEV_BUFFER_COUNT 0
#define LV_LINUX_FBDEV_BUFFER_SIZE 60
#define LV_LINUX_FBDEV_MMAP 1
#endif
#define LV_USE_LINUX_DRM 0
#define LV_USE_EVDEV 0
#define LV_USE_SDL 0

#define LV_DEF_REFR_PERIOD 16

/* Text is drawn with FreeType (DejaVu Sans Condensed). Built-in Montserrat is the fallback. */
#define LV_USE_FREETYPE 1
#define LV_FREETYPE_CACHE_FT_GLYPH_CNT 256
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_USE_FONT_PLACEHOLDER 1

#define LV_USE_THEME_DEFAULT 0
#define LV_USE_LABEL 1
#define LV_USE_IMAGE 1
#define LV_USE_CANVAS 1
#define LV_USE_FLEX 1
#define LV_USE_SNAPSHOT 1

#define LV_USE_PERF_MONITOR 0
#define LV_USE_SYSMON 0

#endif /* LV_CONF_H */
