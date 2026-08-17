#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_48 1

#define LV_USE_QRCODE 1
#define LV_USE_LODEPNG 1

// LVGL's bundled lodepng allocates through lv_malloc/lv_realloc. Decoding a
// 64x64 airline logo needs more than the 64KB built-in pool has spare, so use
// the C library allocator and the far larger ESP32 heap instead. This also
// gives back the static pool.
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#endif
