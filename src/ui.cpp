#include "ui.h"
#include "config.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <time.h>
#include <lvgl.h>

extern Arduino_ESP32RGBPanel* displayBus;
extern Arduino_ST7701_RGBPanel* panel;
extern Arduino_GFX* gfx;
extern lv_disp_t* disp;
extern lv_indev_t* indev;

static lv_obj_t* temp_label;
static lv_obj_t* humidity_label;
static lv_obj_t* pressure_label;
static lv_obj_t* wind_label;
static lv_obj_t* wind_direction_icon_obj;
static lv_obj_t* rain_label;
static lv_obj_t* sunrise_value_label;
static lv_obj_t* sunset_value_label;
static lv_obj_t* sunrise_label;
static lv_obj_t* sunset_label;
static lv_obj_t* moon_phase_label;
static lv_obj_t* moon_canvas;
static lv_obj_t* time_label;
static lv_obj_t* date_label;
static lv_obj_t* refresh_label;
static lv_obj_t* wifi_status_label;
static lv_obj_t* status_label;
static lv_obj_t* station_label;
static lv_obj_t* page_indicator_label;
static lv_obj_t* portal_page_overlay;
static lv_obj_t* portal_page_ssid_label;
static lv_obj_t* portal_page_url_label;
static lv_obj_t* portal_page_qr;
static lv_obj_t* setup_overlay;
static lv_obj_t* setup_wifi_label;
static lv_obj_t* setup_url_label;
static lv_obj_t* setup_wifi_qr;
static lv_obj_t* setup_url_qr;
static constexpr int MOON_CANVAS_SIZE = 64;
static uint8_t moon_canvas_buffer[LV_CANVAS_BUF_SIZE(MOON_CANVAS_SIZE, MOON_CANVAS_SIZE, 32, LV_DRAW_BUF_STRIDE_ALIGN)];

static lv_style_t screen_style;
static lv_style_t card_style;
static lv_style_t hero_card_style;
static lv_style_t eyebrow_style;
static lv_style_t title_style;
static lv_style_t hero_value_style;
static lv_style_t metric_value_style;
static lv_style_t compact_value_style;
static lv_style_t time_style;
static lv_style_t date_style;
static lv_style_t refresh_style;
static lv_style_t status_style;
static lv_style_t unit_style;
static lv_style_t chip_style;
static lv_obj_t* screen_bg;
static lv_obj_t* bg_glow_top_left;
static lv_obj_t* bg_glow_bottom_right;
static lv_obj_t* bg_glow_top_mid;
static lv_obj_t* location_card_obj;
static lv_obj_t* clock_card_obj;
static lv_obj_t* hero_card_obj;
static lv_obj_t* humidity_card_obj;
static lv_obj_t* wind_card_obj;
static lv_obj_t* rain_card_obj;
static lv_obj_t* pressure_card_obj;
static lv_obj_t* sun_card_obj;
static lv_obj_t* moon_card_obj;
static lv_obj_t* clock_accent_obj;
static lv_obj_t* hero_accent_obj;
static lv_obj_t* humidity_accent_obj;
static lv_obj_t* wind_accent_obj;
static lv_obj_t* rain_accent_obj;
static lv_obj_t* pressure_accent_obj;
static lv_obj_t* sun_accent_obj;
static lv_obj_t* moon_accent_obj;
static lv_obj_t* clock_chip_obj;
static lv_obj_t* hero_chip_obj;
static lv_obj_t* humidity_chip_obj;
static lv_obj_t* wind_chip_obj;
static lv_obj_t* rain_chip_obj;
static lv_obj_t* pressure_chip_obj;
static lv_obj_t* sun_chip_obj;
static lv_obj_t* moon_chip_obj;
static constexpr uint32_t DISPLAY_DRAW_BUFFER_LINES = 32;
static uint16_t lvgl_draw_buffer[DISPLAY_WIDTH * DISPLAY_DRAW_BUFFER_LINES];

struct PageTheme {
    uint32_t screenTop;
    uint32_t screenBottom;
    uint32_t glowTopLeft;
    uint32_t glowBottomRight;
    uint32_t glowTopMid;
    uint32_t cardTop;
    uint32_t cardBottom;
    uint32_t cardBorder;
    uint32_t heroTop;
    uint32_t heroBottom;
    uint32_t heroBorder;
    uint32_t primaryAccent;
    uint32_t secondaryAccent;
    uint32_t tertiaryAccent;
    uint32_t titleColor;
};

static constexpr PageTheme PAGE_THEMES[] = {
    {0x07131D, 0x0E2232, 0x0E8FB0, 0x185B8A, 0xF0B34A, 0x102435, 0x17344B, 0x21465F, 0x11445D, 0x1D6987, 0x347A9B, 0x6BD5F9, 0x58D6C9, 0xF5D07A, 0x9FD8EF},
    {0x081A17, 0x0E2E29, 0x18A57D, 0x1B756A, 0xE1D16A, 0x102B28, 0x17403B, 0x255550, 0x125449, 0x1C7A67, 0x3A8D7C, 0x69E2C2, 0x7FD8A7, 0xE8DA7A, 0xB9F0DF},
    {0x171106, 0x332310, 0xD97C2B, 0x8D4C18, 0xF3D68A, 0x312112, 0x47301B, 0x6B4A2A, 0x6A4018, 0x9B6327, 0xB97A39, 0xF3B25C, 0xF2C96C, 0xFFD98C, 0xFFE2A8},
    {0x1A0E12, 0x311A21, 0xC95A63, 0x7F3347, 0xF1A86B, 0x2D1820, 0x44232D, 0x6A3745, 0x70303D, 0x9C475C, 0xB85F73, 0xF28C8A, 0xF0B56B, 0xF2C3A0, 0xFFD6D0}
};

enum class ChipIcon {
    humidity,
    wind,
    rain,
    pressure,
    sun,
    moon,
    clock,
    station
};

static void draw_moon_phase(float phase);
static const char* moon_phase_name(float phase);
static void request_full_redraw();
static void configure_panel_variant();
static lv_obj_t* create_glow(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t size, uint32_t color, lv_opa_t opa);
static lv_obj_t* create_card_accent(lv_obj_t* card, lv_coord_t width, uint32_t color);
static lv_obj_t* create_icon_chip(lv_obj_t* parent, ChipIcon icon, lv_coord_t x, lv_coord_t y, uint32_t color);
static void create_icon_element(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, uint32_t color, lv_opa_t opa = LV_OPA_COVER, lv_coord_t radius = LV_RADIUS_CIRCLE);
static void populate_icon_chip(lv_obj_t* chip, ChipIcon icon);
static lv_obj_t* create_wind_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_obj_t** value_label, lv_obj_t** accent_obj, lv_obj_t** chip_obj);
static void ensure_portal_page_overlay();
static void ensure_setup_overlay();
static void apply_page_theme(uint8_t page_index);

static lv_obj_t* create_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, const char* title, ChipIcon icon, uint32_t accent_color, lv_obj_t** value_label, lv_obj_t** accent_obj, lv_obj_t** chip_obj) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_add_style(card, &card_style, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    *accent_obj = create_card_accent(card, w, accent_color);
    *chip_obj = create_icon_chip(card, icon, w - 34, 18, accent_color);

    lv_obj_t* title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_add_style(title_label, &title_style, 0);
    lv_obj_set_width(title_label, w - 50);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 14, 18);

    *value_label = lv_label_create(card);
    lv_label_set_text(*value_label, "--");
    lv_obj_add_style(*value_label, &metric_value_style, 0);
    lv_obj_set_width(*value_label, w - 28);
    lv_label_set_long_mode(*value_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(*value_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(*value_label, LV_ALIGN_CENTER, 0, 26);

    return card;
}

static lv_obj_t* create_sun_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_obj_t** accent_obj, lv_obj_t** chip_obj) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_add_style(card, &card_style, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    *accent_obj = create_card_accent(card, w, 0xF0B34A);
    *chip_obj = create_icon_chip(card, ChipIcon::sun, w - 34, 18, 0xF0B34A);

    lv_obj_t* title_label = lv_label_create(card);
    lv_label_set_text(title_label, "Sun");
    lv_obj_add_style(title_label, &title_style, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 14, 18);

    sunrise_label = lv_label_create(card);
    lv_label_set_text(sunrise_label, "Rise");
    lv_obj_add_style(sunrise_label, &unit_style, 0);
    lv_obj_align(sunrise_label, LV_ALIGN_TOP_LEFT, 14, 56);

    sunrise_value_label = lv_label_create(card);
    lv_label_set_text(sunrise_value_label, "--:--");
    lv_obj_add_style(sunrise_value_label, &compact_value_style, 0);
    lv_obj_align(sunrise_value_label, LV_ALIGN_TOP_RIGHT, -14, 52);

    sunset_label = lv_label_create(card);
    lv_label_set_text(sunset_label, "Set");
    lv_obj_add_style(sunset_label, &unit_style, 0);
    lv_obj_align(sunset_label, LV_ALIGN_TOP_LEFT, 14, 92);

    sunset_value_label = lv_label_create(card);
    lv_label_set_text(sunset_value_label, "--:--");
    lv_obj_add_style(sunset_value_label, &compact_value_style, 0);
    lv_obj_align(sunset_value_label, LV_ALIGN_TOP_RIGHT, -14, 88);

    return card;
}

static lv_obj_t* create_wind_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_obj_t** value_label, lv_obj_t** accent_obj, lv_obj_t** chip_obj) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_add_style(card, &card_style, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    *accent_obj = create_card_accent(card, w, 0x7CB9FF);
    *chip_obj = create_icon_chip(card, ChipIcon::wind, w - 34, 18, 0x7CB9FF);

    lv_obj_t* title_label = lv_label_create(card);
    lv_label_set_text(title_label, "Wind (m/s)");
    lv_obj_add_style(title_label, &title_style, 0);
    lv_obj_set_width(title_label, w - 50);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 14, 18);

    wind_direction_icon_obj = lv_obj_create(card);
    lv_obj_remove_style_all(wind_direction_icon_obj);
    lv_obj_set_size(wind_direction_icon_obj, 44, 44);
    lv_obj_set_style_transform_pivot_x(wind_direction_icon_obj, 22, 0);
    lv_obj_set_style_transform_pivot_y(wind_direction_icon_obj, 22, 0);
    lv_obj_align(wind_direction_icon_obj, LV_ALIGN_TOP_MID, 0, 44);

    lv_obj_t* needle_head = lv_label_create(wind_direction_icon_obj);
    lv_label_set_text(needle_head, LV_SYMBOL_UP);
    lv_obj_set_style_text_font(needle_head, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(needle_head, lv_color_hex(0xD8ECFF), 0);
    lv_obj_align(needle_head, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* needle_shaft = lv_obj_create(wind_direction_icon_obj);
    lv_obj_remove_style_all(needle_shaft);
    lv_obj_set_size(needle_shaft, 4, 18);
    lv_obj_set_style_bg_color(needle_shaft, lv_color_hex(0xBFDFFF), 0);
    lv_obj_set_style_bg_opa(needle_shaft, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(needle_shaft, 2, 0);
    lv_obj_align(needle_shaft, LV_ALIGN_CENTER, 0, -2);

    lv_obj_t* needle_tail = lv_obj_create(wind_direction_icon_obj);
    lv_obj_remove_style_all(needle_tail);
    lv_obj_set_size(needle_tail, 4, 10);
    lv_obj_set_style_bg_color(needle_tail, lv_color_hex(0x7EA8CF), 0);
    lv_obj_set_style_bg_opa(needle_tail, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(needle_tail, 2, 0);
    lv_obj_align(needle_tail, LV_ALIGN_BOTTOM_MID, 0, -4);

    lv_obj_t* needle_hub = lv_obj_create(wind_direction_icon_obj);
    lv_obj_remove_style_all(needle_hub);
    lv_obj_set_size(needle_hub, 8, 8);
    lv_obj_set_style_bg_color(needle_hub, lv_color_hex(0xE6F3FF), 0);
    lv_obj_set_style_bg_opa(needle_hub, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(needle_hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(needle_hub, LV_ALIGN_CENTER, 0, 8);

    *value_label = lv_label_create(card);
    lv_label_set_text(*value_label, "--.-");
    lv_obj_add_style(*value_label, &metric_value_style, 0);
    lv_obj_set_width(*value_label, w - 28);
    lv_label_set_long_mode(*value_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(*value_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(*value_label, LV_ALIGN_BOTTOM_MID, 0, -14);

    return card;
}

static lv_obj_t* create_moon_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_obj_t** accent_obj, lv_obj_t** chip_obj) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_add_style(card, &card_style, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    *accent_obj = create_card_accent(card, w, 0xB7CCE8);
    *chip_obj = create_icon_chip(card, ChipIcon::moon, w - 34, 18, 0xB7CCE8);

    lv_obj_t* title_label = lv_label_create(card);
    lv_label_set_text(title_label, "Moon");
    lv_obj_add_style(title_label, &title_style, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 14, 18);

    moon_canvas = lv_canvas_create(card);
    lv_obj_remove_style_all(moon_canvas);
    lv_canvas_set_buffer(moon_canvas, moon_canvas_buffer, MOON_CANVAS_SIZE, MOON_CANVAS_SIZE, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_align(moon_canvas, LV_ALIGN_CENTER, 0, -2);
    draw_moon_phase(0.0f);

    moon_phase_label = lv_label_create(card);
    lv_label_set_text(moon_phase_label, "New Moon");
    lv_obj_add_style(moon_phase_label, &unit_style, 0);
    lv_obj_set_style_text_font(moon_phase_label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(moon_phase_label, w - 28);
    lv_label_set_long_mode(moon_phase_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(moon_phase_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(moon_phase_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    return card;
}

static lv_obj_t* create_glow(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t size, uint32_t color, lv_opa_t opa) {
    lv_obj_t* glow = lv_obj_create(parent);
    lv_obj_remove_style_all(glow);
    lv_obj_set_size(glow, size, size);
    lv_obj_set_pos(glow, x, y);
    lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(glow, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(glow, opa, 0);
    lv_obj_set_style_border_width(glow, 0, 0);
    return glow;
}

static lv_obj_t* create_card_accent(lv_obj_t* card, lv_coord_t width, uint32_t color) {
    lv_obj_t* accent = lv_obj_create(card);
    lv_obj_remove_style_all(accent);
    lv_obj_set_size(accent, width - 28, 4);
    lv_obj_set_pos(accent, 14, 12);
    lv_obj_set_style_radius(accent, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(accent, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_80, 0);
    return accent;
}

static void create_icon_element(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, uint32_t color, lv_opa_t opa, lv_coord_t radius) {
    lv_obj_t* part = lv_obj_create(parent);
    lv_obj_remove_style_all(part);
    lv_obj_set_size(part, w, h);
    lv_obj_set_pos(part, x, y);
    lv_obj_set_style_bg_color(part, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(part, opa, 0);
    lv_obj_set_style_radius(part, radius, 0);
    lv_obj_set_style_border_width(part, 0, 0);
}

static void populate_icon_chip(lv_obj_t* chip, ChipIcon icon) {
    const uint32_t ink = 0x041018;
    const uint32_t cut = 0x17344B;

    switch (icon) {
        case ChipIcon::humidity:
            create_icon_element(chip, 10, 4, 4, 4, ink);
            create_icon_element(chip, 8, 7, 8, 8, ink);
            create_icon_element(chip, 7, 11, 10, 10, ink);
            break;
        case ChipIcon::wind:
            create_icon_element(chip, 5, 7, 12, 2, ink, LV_OPA_COVER, 2);
            create_icon_element(chip, 8, 11, 9, 2, ink, LV_OPA_COVER, 2);
            create_icon_element(chip, 6, 15, 11, 2, ink, LV_OPA_COVER, 2);
            create_icon_element(chip, 16, 6, 3, 3, ink);
            create_icon_element(chip, 15, 14, 4, 4, ink);
            break;
        case ChipIcon::rain:
            create_icon_element(chip, 6, 8, 12, 6, ink, LV_OPA_COVER, 6);
            create_icon_element(chip, 8, 5, 6, 6, ink);
            create_icon_element(chip, 12, 6, 5, 5, ink);
            create_icon_element(chip, 8, 15, 2, 5, ink, LV_OPA_COVER, 2);
            create_icon_element(chip, 12, 16, 2, 4, ink, LV_OPA_COVER, 2);
            create_icon_element(chip, 16, 15, 2, 5, ink, LV_OPA_COVER, 2);
            break;
        case ChipIcon::pressure: {
            lv_obj_t* ring = lv_obj_create(chip);
            lv_obj_remove_style_all(ring);
            lv_obj_set_size(ring, 14, 14);
            lv_obj_set_pos(ring, 5, 6);
            lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(ring, 2, 0);
            lv_obj_set_style_border_color(ring, lv_color_hex(ink), 0);
            create_icon_element(chip, 11, 11, 2, 5, ink, LV_OPA_COVER, 1);
            create_icon_element(chip, 11, 5, 2, 2, ink);
            create_icon_element(chip, 17, 11, 2, 2, ink);
            create_icon_element(chip, 5, 11, 2, 2, ink);
            break;
        }
        case ChipIcon::sun:
            create_icon_element(chip, 8, 8, 8, 8, ink);
            create_icon_element(chip, 11, 3, 2, 3, ink, LV_OPA_COVER, 1);
            create_icon_element(chip, 11, 18, 2, 3, ink, LV_OPA_COVER, 1);
            create_icon_element(chip, 3, 11, 3, 2, ink, LV_OPA_COVER, 1);
            create_icon_element(chip, 18, 11, 3, 2, ink, LV_OPA_COVER, 1);
            break;
        case ChipIcon::moon:
            create_icon_element(chip, 6, 5, 12, 12, ink);
            create_icon_element(chip, 10, 5, 10, 12, cut);
            break;
        case ChipIcon::clock: {
            lv_obj_t* ring = lv_obj_create(chip);
            lv_obj_remove_style_all(ring);
            lv_obj_set_size(ring, 14, 14);
            lv_obj_set_pos(ring, 5, 5);
            lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(ring, 2, 0);
            lv_obj_set_style_border_color(ring, lv_color_hex(ink), 0);
            create_icon_element(chip, 11, 8, 2, 5, ink, LV_OPA_COVER, 1);
            create_icon_element(chip, 11, 11, 4, 2, ink, LV_OPA_COVER, 1);
            break;
        }
        case ChipIcon::station:
            create_icon_element(chip, 9, 4, 6, 6, ink);
            create_icon_element(chip, 8, 8, 8, 8, ink);
            create_icon_element(chip, 10, 17, 4, 3, ink, LV_OPA_COVER, 2);
            create_icon_element(chip, 11, 10, 2, 2, 0xF5D07A);
            break;
    }
}

static lv_obj_t* create_icon_chip(lv_obj_t* parent, ChipIcon icon, lv_coord_t x, lv_coord_t y, uint32_t color) {
    lv_obj_t* chip = lv_obj_create(parent);
    lv_obj_set_size(chip, 24, 24);
    lv_obj_set_pos(chip, x, y);
    lv_obj_add_style(chip, &chip_style, 0);
    lv_obj_set_style_bg_color(chip, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chip, lv_color_hex(color), 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    populate_icon_chip(chip, icon);
    return chip;
}

static void apply_card_theme(lv_obj_t* card, const PageTheme& theme) {
    if (card == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(card, lv_color_hex(theme.cardTop), 0);
    lv_obj_set_style_bg_grad_color(card, lv_color_hex(theme.cardBottom), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(theme.cardBorder), 0);
}

static void apply_chip_theme(lv_obj_t* chip, uint32_t color) {
    if (chip == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(chip, lv_color_hex(color), 0);
    lv_obj_set_style_border_color(chip, lv_color_hex(color), 0);
}

static void apply_accent_theme(lv_obj_t* accent, uint32_t color) {
    if (accent == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(accent, lv_color_hex(color), 0);
}

static void apply_page_theme(uint8_t page_index) {
    const PageTheme& theme = PAGE_THEMES[page_index % (sizeof(PAGE_THEMES) / sizeof(PAGE_THEMES[0]))];

    if (screen_bg != nullptr) {
        lv_obj_set_style_bg_color(screen_bg, lv_color_hex(theme.screenTop), 0);
        lv_obj_set_style_bg_grad_color(screen_bg, lv_color_hex(theme.screenBottom), 0);
    }

    if (bg_glow_top_left != nullptr) {
        lv_obj_set_style_bg_color(bg_glow_top_left, lv_color_hex(theme.glowTopLeft), 0);
    }
    if (bg_glow_bottom_right != nullptr) {
        lv_obj_set_style_bg_color(bg_glow_bottom_right, lv_color_hex(theme.glowBottomRight), 0);
    }
    if (bg_glow_top_mid != nullptr) {
        lv_obj_set_style_bg_color(bg_glow_top_mid, lv_color_hex(theme.glowTopMid), 0);
    }

    apply_card_theme(location_card_obj, theme);
    apply_card_theme(clock_card_obj, theme);
    apply_card_theme(humidity_card_obj, theme);
    apply_card_theme(wind_card_obj, theme);
    apply_card_theme(rain_card_obj, theme);
    apply_card_theme(pressure_card_obj, theme);
    apply_card_theme(sun_card_obj, theme);
    apply_card_theme(moon_card_obj, theme);

    if (hero_card_obj != nullptr) {
        lv_obj_set_style_bg_color(hero_card_obj, lv_color_hex(theme.heroTop), 0);
        lv_obj_set_style_bg_grad_color(hero_card_obj, lv_color_hex(theme.heroBottom), 0);
        lv_obj_set_style_border_color(hero_card_obj, lv_color_hex(theme.heroBorder), 0);
    }

    apply_accent_theme(clock_accent_obj, theme.primaryAccent);
    apply_accent_theme(hero_accent_obj, theme.tertiaryAccent);
    apply_accent_theme(humidity_accent_obj, theme.secondaryAccent);
    apply_accent_theme(wind_accent_obj, theme.primaryAccent);
    apply_accent_theme(rain_accent_obj, theme.primaryAccent);
    apply_accent_theme(pressure_accent_obj, theme.tertiaryAccent);
    apply_accent_theme(sun_accent_obj, theme.tertiaryAccent);
    apply_accent_theme(moon_accent_obj, theme.secondaryAccent);

    apply_chip_theme(clock_chip_obj, theme.primaryAccent);
    apply_chip_theme(hero_chip_obj, theme.tertiaryAccent);
    apply_chip_theme(humidity_chip_obj, theme.secondaryAccent);
    apply_chip_theme(wind_chip_obj, theme.primaryAccent);
    apply_chip_theme(rain_chip_obj, theme.primaryAccent);
    apply_chip_theme(pressure_chip_obj, theme.tertiaryAccent);
    apply_chip_theme(sun_chip_obj, theme.tertiaryAccent);
    apply_chip_theme(moon_chip_obj, theme.secondaryAccent);

    if (station_label != nullptr) {
        lv_obj_set_style_text_color(station_label, lv_color_hex(theme.titleColor), 0);
    }
    if (page_indicator_label != nullptr) {
        lv_obj_set_style_text_color(page_indicator_label, lv_color_hex(theme.titleColor), 0);
    }
}

static void init_display() {
    if (gfx != nullptr) {
        return;
    }

    displayBus = new Arduino_ESP32RGBPanel(
        DISPLAY_PIN_CS,
        DISPLAY_PIN_SCK,
        DISPLAY_PIN_MOSI,
        DISPLAY_PIN_DE,
        DISPLAY_PIN_VSYNC,
        DISPLAY_PIN_HSYNC,
        DISPLAY_PIN_PCLK,
        DISPLAY_PIN_R0,
        DISPLAY_PIN_R1,
        DISPLAY_PIN_R2,
        DISPLAY_PIN_R3,
        DISPLAY_PIN_R4,
        DISPLAY_PIN_G0,
        DISPLAY_PIN_G1,
        DISPLAY_PIN_G2,
        DISPLAY_PIN_G3,
        DISPLAY_PIN_G4,
        DISPLAY_PIN_G5,
        DISPLAY_PIN_B0,
        DISPLAY_PIN_B1,
        DISPLAY_PIN_B2,
        DISPLAY_PIN_B3,
        DISPLAY_PIN_B4,
        false
    );

    panel = new Arduino_ST7701_RGBPanel(
        displayBus,
        DISPLAY_PIN_RST,
        0,
        true,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT,
        st7701_type1_init_operations,
        sizeof(st7701_type1_init_operations),
        false,
        DISPLAY_HSYNC_FRONT_PORCH,
        DISPLAY_HSYNC_PULSE_WIDTH,
        DISPLAY_HSYNC_BACK_PORCH,
        DISPLAY_VSYNC_FRONT_PORCH,
        DISPLAY_VSYNC_PULSE_WIDTH,
        DISPLAY_VSYNC_BACK_PORCH
    );
    gfx = panel;
}

static uint32_t tick_millis() {
    return static_cast<uint32_t>(millis());
}

static const char* wind_cardinal(float degrees) {
    static const char* directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};

    while (degrees < 0.0f) {
        degrees += 360.0f;
    }
    while (degrees >= 360.0f) {
        degrees -= 360.0f;
    }

    int index = static_cast<int>((degrees + 22.5f) / 45.0f) % 8;
    return directions[index];
}

static void update_wind_direction_icon(float degrees) {
    if (wind_direction_icon_obj == nullptr) {
        return;
    }

    while (degrees < 0.0f) {
        degrees += 360.0f;
    }
    while (degrees >= 360.0f) {
        degrees -= 360.0f;
    }

    lv_obj_set_style_transform_rotation(
        wind_direction_icon_obj,
        static_cast<int32_t>(degrees * 10.0f),
        0
    );
}

static void draw_moon_phase(float phase) {
    if (moon_canvas == nullptr) {
        return;
    }

    lv_canvas_fill_bg(moon_canvas, lv_color_hex(0x000000), LV_OPA_TRANSP);

    const int center = MOON_CANVAS_SIZE / 2;
    const int radius = (MOON_CANVAS_SIZE / 2) - 2;
    const float cycle = phase * 2.0f * PI;
    const float terminator = cosf(cycle);
    const bool waxing = phase <= 0.5f;
    const lv_color_t lit = lv_color_hex(0xF4E7BF);
    const lv_color_t shadow = lv_color_hex(0x55697A);

    for (int y = 0; y < MOON_CANVAS_SIZE; ++y) {
        for (int x = 0; x < MOON_CANVAS_SIZE; ++x) {
            float nx = static_cast<float>(x - center) / radius;
            float ny = static_cast<float>(y - center) / radius;
            float rr = (nx * nx) + (ny * ny);
            if (rr > 1.0f) {
                continue;
            }

            float xr = sqrtf(1.0f - (ny * ny));
            bool illuminated = waxing ? (nx > terminator * xr) : (nx < (-terminator * xr));
            lv_canvas_set_px(moon_canvas, x, y, illuminated ? lit : shadow, LV_OPA_COVER);
        }
    }
}

static const char* moon_phase_name(float phase) {
    while (phase < 0.0f) {
        phase += 1.0f;
    }
    while (phase >= 1.0f) {
        phase -= 1.0f;
    }

    if (phase < 0.0625f || phase >= 0.9375f) {
        return "New Moon";
    }
    if (phase < 0.1875f) {
        return "Waxing Crescent";
    }
    if (phase < 0.3125f) {
        return "First Quarter";
    }
    if (phase < 0.4375f) {
        return "Waxing Gibbous";
    }
    if (phase < 0.5625f) {
        return "Full Moon";
    }
    if (phase < 0.6875f) {
        return "Waning Gibbous";
    }
    if (phase < 0.8125f) {
        return "Third Quarter";
    }
    return "Waning Crescent";
}

static void flush_display(lv_display_t* display, const lv_area_t* area, uint8_t* px_map) {
    uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
    uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);
    gfx->draw16bitRGBBitmap(
        area->x1,
        area->y1,
        reinterpret_cast<uint16_t*>(px_map),
        width,
        height
    );
    lv_display_flush_ready(display);
}

static void read_touch(lv_indev_t* input_device, lv_indev_data_t* data) {
    LV_UNUSED(input_device);
    data->state = LV_INDEV_STATE_RELEASED;
}

static void configure_panel_variant() {
    // Guition/ESP32-4848S040 variants commonly need MDT disabled after init.
    displayBus->beginWrite();
    displayBus->writeCommand(0xFF);
    displayBus->write(0x77);
    displayBus->write(0x01);
    displayBus->write(0x00);
    displayBus->write(0x00);
    displayBus->write(0x10);
    displayBus->writeCommand(0xCD);
    displayBus->write(0x00);
    displayBus->endWrite();
}

static void request_full_redraw() {
    if (disp == nullptr) {
        return;
    }

    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(disp);
}

static void ensure_portal_page_overlay() {
    if (portal_page_overlay != nullptr) {
        return;
    }

    portal_page_overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(portal_page_overlay);
    lv_obj_set_size(portal_page_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(portal_page_overlay, lv_color_hex(0x08141C), 0);
    lv_obj_set_style_bg_grad_color(portal_page_overlay, lv_color_hex(0x0F2736), 0);
    lv_obj_set_style_bg_grad_dir(portal_page_overlay, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(portal_page_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(portal_page_overlay, 0, 0);
    lv_obj_clear_flag(portal_page_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(portal_page_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* panel_box = lv_obj_create(portal_page_overlay);
    lv_obj_set_size(panel_box, 464, 464);
    lv_obj_center(panel_box);
    lv_obj_set_style_bg_color(panel_box, lv_color_hex(0x102435), 0);
    lv_obj_set_style_bg_grad_color(panel_box, lv_color_hex(0x18384B), 0);
    lv_obj_set_style_bg_grad_dir(panel_box, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(panel_box, lv_color_hex(0x2E5A72), 0);
    lv_obj_set_style_border_width(panel_box, 1, 0);
    lv_obj_set_style_radius(panel_box, 28, 0);
    lv_obj_set_style_pad_all(panel_box, 0, 0);
    lv_obj_clear_flag(panel_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(panel_box);
    lv_label_set_text(title, "Setup Page");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 22);

    lv_obj_t* subtitle = lv_label_create(panel_box);
    lv_label_set_text(subtitle, "Scan to open the setup page while your phone is on the same Wi-Fi network.");
    lv_obj_set_width(subtitle, 408);
    lv_label_set_long_mode(subtitle, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xA9C8D8), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 24, 68);

    lv_obj_t* network_card = lv_obj_create(panel_box);
    lv_obj_set_size(network_card, 196, 164);
    lv_obj_set_pos(network_card, 24, 146);
    lv_obj_set_style_bg_color(network_card, lv_color_hex(0x0D1D28), 0);
    lv_obj_set_style_border_color(network_card, lv_color_hex(0x255068), 0);
    lv_obj_set_style_border_width(network_card, 1, 0);
    lv_obj_set_style_radius(network_card, 20, 0);
    lv_obj_set_style_pad_all(network_card, 0, 0);
    lv_obj_clear_flag(network_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* network_title = lv_label_create(network_card);
    lv_label_set_text(network_title, "Connected Network");
    lv_obj_set_width(network_title, 164);
    lv_label_set_long_mode(network_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(network_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(network_title, lv_color_white(), 0);
    lv_obj_align(network_title, LV_ALIGN_TOP_LEFT, 16, 14);

    portal_page_ssid_label = lv_label_create(network_card);
    lv_label_set_text(portal_page_ssid_label, "SSID");
    lv_obj_set_width(portal_page_ssid_label, 164);
    lv_label_set_long_mode(portal_page_ssid_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(portal_page_ssid_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(portal_page_ssid_label, lv_color_hex(0xDDECF5), 0);
    lv_obj_align(portal_page_ssid_label, LV_ALIGN_TOP_LEFT, 16, 54);

    lv_obj_t* network_hint = lv_label_create(network_card);
    lv_label_set_text(network_hint, "Open the setup page from this Wi-Fi to change settings.");
    lv_obj_set_width(network_hint, 164);
    lv_label_set_long_mode(network_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(network_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(network_hint, lv_color_hex(0x96C2DA), 0);
    lv_obj_align(network_hint, LV_ALIGN_TOP_LEFT, 16, 94);

    lv_obj_t* qr_card = lv_obj_create(panel_box);
    lv_obj_set_size(qr_card, 220, 268);
    lv_obj_set_pos(qr_card, 220, 146);
    lv_obj_set_style_bg_color(qr_card, lv_color_hex(0x0D1D28), 0);
    lv_obj_set_style_border_color(qr_card, lv_color_hex(0x255068), 0);
    lv_obj_set_style_border_width(qr_card, 1, 0);
    lv_obj_set_style_radius(qr_card, 20, 0);
    lv_obj_set_style_pad_all(qr_card, 0, 0);
    lv_obj_clear_flag(qr_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* qr_title = lv_label_create(qr_card);
    lv_label_set_text(qr_title, "Open Setup Page");
    lv_obj_set_style_text_font(qr_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(qr_title, lv_color_white(), 0);
    lv_obj_align(qr_title, LV_ALIGN_TOP_LEFT, 16, 14);

    portal_page_url_label = lv_label_create(qr_card);
    lv_label_set_text(portal_page_url_label, "http://0.0.0.0");
    lv_obj_set_width(portal_page_url_label, 188);
    lv_label_set_long_mode(portal_page_url_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(portal_page_url_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(portal_page_url_label, lv_color_hex(0x96C2DA), 0);
    lv_obj_align(portal_page_url_label, LV_ALIGN_TOP_LEFT, 16, 46);

    portal_page_qr = lv_qrcode_create(qr_card);
    lv_qrcode_set_size(portal_page_qr, 148);
    lv_qrcode_set_dark_color(portal_page_qr, lv_color_hex(0x06121A));
    lv_qrcode_set_light_color(portal_page_qr, lv_color_hex(0xF5FBFF));
    lv_obj_align(portal_page_qr, LV_ALIGN_BOTTOM_MID, 0, -18);

    lv_obj_t* footer = lv_label_create(panel_box);
    lv_label_set_text(footer, "Swipe right to return");
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(footer, lv_color_hex(0x96C2DA), 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, 24, -18);
}

static void ensure_setup_overlay() {
    if (setup_overlay != nullptr) {
        return;
    }

    setup_overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(setup_overlay);
    lv_obj_set_size(setup_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(setup_overlay, lv_color_hex(0x08141C), 0);
    lv_obj_set_style_bg_grad_color(setup_overlay, lv_color_hex(0x0F2736), 0);
    lv_obj_set_style_bg_grad_dir(setup_overlay, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(setup_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(setup_overlay, 0, 0);
    lv_obj_clear_flag(setup_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* panel_box = lv_obj_create(setup_overlay);
    lv_obj_set_size(panel_box, 464, 464);
    lv_obj_center(panel_box);
    lv_obj_set_style_bg_color(panel_box, lv_color_hex(0x102435), 0);
    lv_obj_set_style_bg_grad_color(panel_box, lv_color_hex(0x18384B), 0);
    lv_obj_set_style_bg_grad_dir(panel_box, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(panel_box, lv_color_hex(0x2E5A72), 0);
    lv_obj_set_style_border_width(panel_box, 1, 0);
    lv_obj_set_style_radius(panel_box, 28, 0);
    lv_obj_set_style_pad_all(panel_box, 0, 0);
    lv_obj_clear_flag(panel_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(panel_box);
    lv_label_set_text(title, "Setup Required");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 22);

    lv_obj_t* subtitle = lv_label_create(panel_box);
    lv_label_set_text(subtitle, "1. Join the setup Wi-Fi\n2. Scan the page QR code\n3. Save your settings");
    lv_obj_set_width(subtitle, 408);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xA9C8D8), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 24, 68);

    lv_obj_t* wifi_card = lv_obj_create(panel_box);
    lv_obj_set_size(wifi_card, 198, 274);
    lv_obj_set_pos(wifi_card, 24, 154);
    lv_obj_set_style_bg_color(wifi_card, lv_color_hex(0x0D1D28), 0);
    lv_obj_set_style_border_color(wifi_card, lv_color_hex(0x255068), 0);
    lv_obj_set_style_border_width(wifi_card, 1, 0);
    lv_obj_set_style_radius(wifi_card, 20, 0);
    lv_obj_set_style_pad_all(wifi_card, 0, 0);
    lv_obj_clear_flag(wifi_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* wifi_title = lv_label_create(wifi_card);
    lv_label_set_text(wifi_title, "Join Wi-Fi");
    lv_obj_set_style_text_font(wifi_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(wifi_title, lv_color_white(), 0);
    lv_obj_align(wifi_title, LV_ALIGN_TOP_LEFT, 16, 14);

    setup_wifi_label = lv_label_create(wifi_card);
    lv_label_set_text(setup_wifi_label, "SSID");
    lv_obj_set_width(setup_wifi_label, 166);
    lv_obj_set_style_text_font(setup_wifi_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(setup_wifi_label, lv_color_hex(0x96C2DA), 0);
    lv_obj_align(setup_wifi_label, LV_ALIGN_TOP_LEFT, 16, 46);

    setup_wifi_qr = lv_qrcode_create(wifi_card);
    lv_qrcode_set_size(setup_wifi_qr, 136);
    lv_qrcode_set_dark_color(setup_wifi_qr, lv_color_hex(0x06121A));
    lv_qrcode_set_light_color(setup_wifi_qr, lv_color_hex(0xF5FBFF));
    lv_obj_align(setup_wifi_qr, LV_ALIGN_BOTTOM_MID, 0, -14);

    lv_obj_t* url_card = lv_obj_create(panel_box);
    lv_obj_set_size(url_card, 198, 274);
    lv_obj_set_pos(url_card, 242, 154);
    lv_obj_set_style_bg_color(url_card, lv_color_hex(0x0D1D28), 0);
    lv_obj_set_style_border_color(url_card, lv_color_hex(0x255068), 0);
    lv_obj_set_style_border_width(url_card, 1, 0);
    lv_obj_set_style_radius(url_card, 20, 0);
    lv_obj_set_style_pad_all(url_card, 0, 0);
    lv_obj_clear_flag(url_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* url_title = lv_label_create(url_card);
    lv_label_set_text(url_title, "Open Setup Page");
    lv_obj_set_style_text_font(url_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(url_title, lv_color_white(), 0);
    lv_obj_align(url_title, LV_ALIGN_TOP_LEFT, 16, 14);

    setup_url_label = lv_label_create(url_card);
    lv_label_set_text(setup_url_label, "http://192.168.4.1");
    lv_obj_set_width(setup_url_label, 166);
    lv_obj_set_style_text_font(setup_url_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(setup_url_label, lv_color_hex(0x96C2DA), 0);
    lv_obj_align(setup_url_label, LV_ALIGN_TOP_LEFT, 16, 46);

    setup_url_qr = lv_qrcode_create(url_card);
    lv_qrcode_set_size(setup_url_qr, 136);
    lv_qrcode_set_dark_color(setup_url_qr, lv_color_hex(0x06121A));
    lv_qrcode_set_light_color(setup_url_qr, lv_color_hex(0xF5FBFF));
    lv_obj_align(setup_url_qr, LV_ALIGN_BOTTOM_MID, 0, -14);
}

void ui_init() {
    pinMode(DISPLAY_PIN_BCKL, OUTPUT);
    digitalWrite(DISPLAY_PIN_BCKL, HIGH);
    init_display();
    gfx->begin();
    configure_panel_variant();
    gfx->fillScreen(BLACK);

    lv_init();
    lv_tick_set_cb(tick_millis);

    disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, flush_display);
    lv_display_set_buffers(
        disp,
        lvgl_draw_buffer,
        NULL,
        sizeof(lvgl_draw_buffer),
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );

    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, read_touch);

    lv_style_init(&screen_style);
    lv_style_set_bg_color(&screen_style, lv_color_hex(0x07131D));
    lv_style_set_bg_grad_color(&screen_style, lv_color_hex(0x0E2232));
    lv_style_set_bg_grad_dir(&screen_style, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&screen_style, 0);
    lv_style_set_pad_all(&screen_style, 0);

    lv_style_init(&card_style);
    lv_style_set_bg_color(&card_style, lv_color_hex(0x102435));
    lv_style_set_bg_grad_color(&card_style, lv_color_hex(0x17344B));
    lv_style_set_bg_grad_dir(&card_style, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&card_style, 1);
    lv_style_set_border_color(&card_style, lv_color_hex(0x21465F));
    lv_style_set_radius(&card_style, 18);
    lv_style_set_pad_all(&card_style, 0);
    lv_style_set_shadow_width(&card_style, 0);

    lv_style_init(&hero_card_style);
    lv_style_set_bg_color(&hero_card_style, lv_color_hex(0x11445D));
    lv_style_set_bg_grad_color(&hero_card_style, lv_color_hex(0x1D6987));
    lv_style_set_bg_grad_dir(&hero_card_style, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&hero_card_style, 1);
    lv_style_set_border_color(&hero_card_style, lv_color_hex(0x347A9B));
    lv_style_set_radius(&hero_card_style, 24);
    lv_style_set_pad_all(&hero_card_style, 0);

    lv_style_init(&eyebrow_style);
    lv_style_set_text_color(&eyebrow_style, lv_color_hex(0x9FD8EF));
    lv_style_set_text_font(&eyebrow_style, &lv_font_montserrat_16);

    lv_style_init(&title_style);
    lv_style_set_text_color(&title_style, lv_color_hex(0xD4E6F2));
    lv_style_set_text_font(&title_style, &lv_font_montserrat_16);

    lv_style_init(&hero_value_style);
    lv_style_set_text_color(&hero_value_style, lv_color_white());
    lv_style_set_text_font(&hero_value_style, &lv_font_montserrat_48);

    lv_style_init(&metric_value_style);
    lv_style_set_text_color(&metric_value_style, lv_color_white());
    lv_style_set_text_font(&metric_value_style, &lv_font_montserrat_36);

    lv_style_init(&compact_value_style);
    lv_style_set_text_color(&compact_value_style, lv_color_white());
    lv_style_set_text_font(&compact_value_style, &lv_font_montserrat_20);

    lv_style_init(&time_style);
    lv_style_set_text_color(&time_style, lv_color_white());
    lv_style_set_text_font(&time_style, &lv_font_montserrat_36);
    lv_style_set_text_align(&time_style, LV_TEXT_ALIGN_LEFT);

    lv_style_init(&date_style);
    lv_style_set_text_color(&date_style, lv_color_hex(0xB9D3E2));
    lv_style_set_text_font(&date_style, &lv_font_montserrat_16);
    lv_style_set_text_align(&date_style, LV_TEXT_ALIGN_LEFT);

    lv_style_init(&refresh_style);
    lv_style_set_text_color(&refresh_style, lv_color_hex(0x8AABC0));
    lv_style_set_text_font(&refresh_style, &lv_font_montserrat_16);
    lv_style_set_text_align(&refresh_style, LV_TEXT_ALIGN_LEFT);

    lv_style_init(&status_style);
    lv_style_set_text_color(&status_style, lv_color_hex(0xF3C969));
    lv_style_set_text_font(&status_style, &lv_font_montserrat_20);
    lv_style_set_text_align(&status_style, LV_TEXT_ALIGN_RIGHT);

    lv_style_init(&unit_style);
    lv_style_set_text_color(&unit_style, lv_color_hex(0x8AABC0));
    lv_style_set_text_font(&unit_style, &lv_font_montserrat_16);

    lv_style_init(&chip_style);
    lv_style_set_radius(&chip_style, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&chip_style, 1);
    lv_style_set_border_opa(&chip_style, LV_OPA_40);
    lv_style_set_shadow_width(&chip_style, 0);
    lv_style_set_pad_all(&chip_style, 0);

    screen_bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen_bg, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_add_style(screen_bg, &screen_style, 0);
    lv_obj_clear_flag(screen_bg, LV_OBJ_FLAG_SCROLLABLE);

    bg_glow_top_left = create_glow(screen_bg, -54, -48, 180, 0x0E8FB0, LV_OPA_20);
    bg_glow_bottom_right = create_glow(screen_bg, 330, 322, 170, 0x185B8A, LV_OPA_20);
    bg_glow_top_mid = create_glow(screen_bg, 244, -36, 120, 0xF0B34A, LV_OPA_10);

    location_card_obj = lv_obj_create(screen_bg);
    lv_obj_set_pos(location_card_obj, 16, 8);
    lv_obj_set_size(location_card_obj, 448, 32);
    lv_obj_add_style(location_card_obj, &card_style, 0);
    lv_obj_clear_flag(location_card_obj, LV_OBJ_FLAG_SCROLLABLE);

    station_label = lv_label_create(location_card_obj);
    lv_label_set_text(station_label, "Marlow Weather");
    lv_obj_add_style(station_label, &eyebrow_style, 0);
    lv_obj_set_width(station_label, 416);
    lv_label_set_long_mode(station_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(station_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(station_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(station_label, LV_ALIGN_CENTER, 0, 0);

    page_indicator_label = lv_label_create(location_card_obj);
    lv_label_set_text(page_indicator_label, "1/1");
    lv_obj_add_style(page_indicator_label, &unit_style, 0);
    lv_obj_align(page_indicator_label, LV_ALIGN_RIGHT_MID, -14, 0);

    clock_card_obj = lv_obj_create(screen_bg);
    lv_obj_set_pos(clock_card_obj, 16, 44);
    lv_obj_set_size(clock_card_obj, 216, 126);
    lv_obj_add_style(clock_card_obj, &card_style, 0);
    lv_obj_clear_flag(clock_card_obj, LV_OBJ_FLAG_SCROLLABLE);
    clock_accent_obj = create_card_accent(clock_card_obj, 216, 0x6BD5F9);
    clock_chip_obj = create_icon_chip(clock_card_obj, ChipIcon::clock, 178, 18, 0x6BD5F9);

    hero_card_obj = lv_obj_create(screen_bg);
    lv_obj_set_pos(hero_card_obj, 248, 44);
    lv_obj_set_size(hero_card_obj, 216, 126);
    lv_obj_add_style(hero_card_obj, &hero_card_style, 0);
    lv_obj_clear_flag(hero_card_obj, LV_OBJ_FLAG_SCROLLABLE);
    hero_accent_obj = create_card_accent(hero_card_obj, 216, 0xF5D07A);
    hero_chip_obj = create_icon_chip(hero_card_obj, ChipIcon::station, 178, 18, 0xF5D07A);

    lv_obj_t* temperature_title = lv_label_create(hero_card_obj);
    lv_label_set_text(temperature_title, "Temperature");
    lv_obj_add_style(temperature_title, &title_style, 0);
    lv_obj_align(temperature_title, LV_ALIGN_TOP_LEFT, 18, 16);

    time_label = lv_label_create(clock_card_obj);
    lv_label_set_text(time_label, "--:--:--");
    lv_obj_add_style(time_label, &time_style, 0);
    lv_obj_set_width(time_label, 168);
    lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 16, 14);

    date_label = lv_label_create(clock_card_obj);
    lv_label_set_text(date_label, "-- --- ----");
    lv_obj_add_style(date_label, &date_style, 0);
    lv_obj_set_width(date_label, 168);
    lv_obj_align(date_label, LV_ALIGN_TOP_LEFT, 16, 58);

    refresh_label = lv_label_create(clock_card_obj);
    lv_label_set_text(refresh_label, "Refresh --:--");
    lv_obj_add_style(refresh_label, &refresh_style, 0);
    lv_obj_set_width(refresh_label, 146);
    lv_obj_align(refresh_label, LV_ALIGN_BOTTOM_LEFT, 16, -14);

    wifi_status_label = lv_label_create(clock_card_obj);
    lv_label_set_text(wifi_status_label, LV_SYMBOL_WIFI);
    lv_obj_add_style(wifi_status_label, &unit_style, 0);
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xF3C969), 0);
    lv_obj_align(wifi_status_label, LV_ALIGN_BOTTOM_RIGHT, -16, -14);

    status_label = lv_label_create(clock_card_obj);
    lv_label_set_text(status_label, "o");
    lv_obj_add_style(status_label, &status_style, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -16, 16);

    temp_label = lv_label_create(hero_card_obj);
    lv_label_set_text(temp_label, "--.-\xC2\xB0 C");
    lv_obj_add_style(temp_label, &hero_value_style, 0);
    lv_obj_align(temp_label, LV_ALIGN_TOP_LEFT, 18, 46);

    humidity_card_obj = create_card(screen_bg, 16, 174, 140, 147, "Humidity (%)", ChipIcon::humidity, 0x58D6C9, &humidity_label, &humidity_accent_obj, &humidity_chip_obj);
    rain_card_obj = create_card(screen_bg, 170, 174, 140, 147, "Rain (mm)", ChipIcon::rain, 0x69D2FF, &rain_label, &rain_accent_obj, &rain_chip_obj);
    pressure_card_obj = create_card(screen_bg, 324, 174, 140, 147, "Pressure (hPa)", ChipIcon::pressure, 0xF4C76B, &pressure_label, &pressure_accent_obj, &pressure_chip_obj);
    wind_card_obj = create_wind_card(screen_bg, 16, 325, 140, 147, &wind_label, &wind_accent_obj, &wind_chip_obj);
    sun_card_obj = create_sun_card(screen_bg, 170, 325, 140, 147, &sun_accent_obj, &sun_chip_obj);
    moon_card_obj = create_moon_card(screen_bg, 324, 325, 140, 147, &moon_accent_obj, &moon_chip_obj);

    apply_page_theme(0);

    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(disp);
}

void ui_update_weather(const WeatherData& data) {
    if (!data.isValid) {
        ui_show_error("No data available");
        return;
    }

    lv_label_set_text(status_label, "o");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x78D08B), 0);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f\xC2\xB0 C", data.temperature);
    lv_label_set_text(temp_label, buffer);

    snprintf(buffer, sizeof(buffer), "%.0f", data.humidity);
    lv_label_set_text(humidity_label, buffer);

    snprintf(buffer, sizeof(buffer), "%.1f", data.windSpeed);
    lv_label_set_text(wind_label, buffer);
    update_wind_direction_icon(data.windDirection);

    snprintf(buffer, sizeof(buffer), "%.1f", data.rainToday);
    lv_label_set_text(rain_label, buffer);

    const char* pressureTrendSymbol = "=";
    if (data.pressureTrend > 0) {
        pressureTrendSymbol = "^";
    } else if (data.pressureTrend < 0) {
        pressureTrendSymbol = "v";
    }

    snprintf(buffer, sizeof(buffer), "%.0f %s", data.pressure, pressureTrendSymbol);
    lv_label_set_text(pressure_label, buffer);

    LV_UNUSED(buffer);
    request_full_redraw();
}

void ui_update_astronomy(const char* sunrise, const char* sunset, float moon_phase) {
    if (sunrise_value_label != nullptr) {
        lv_label_set_text(sunrise_value_label, sunrise);
    }
    if (sunset_value_label != nullptr) {
        lv_label_set_text(sunset_value_label, sunset);
    }
    if (moon_phase_label != nullptr) {
        lv_label_set_text(moon_phase_label, moon_phase_name(moon_phase));
    }
    draw_moon_phase(moon_phase);
    request_full_redraw();
}

void ui_update_clock(const char* time_text, const char* date_text, unsigned long seconds_until_refresh, bool has_refresh_schedule) {
    if (time_label == nullptr || date_label == nullptr || refresh_label == nullptr) {
        return;
    }

    if (time_text == nullptr || date_text == nullptr) {
        lv_label_set_text(time_label, "--:--:--");
        lv_label_set_text(date_label, "-- --- ----");
        lv_label_set_text(refresh_label, "Refresh --:--");
        return;
    }

    char refresh_buffer[24];
    lv_label_set_text(time_label, time_text);
    lv_label_set_text(date_label, date_text);

    if (has_refresh_schedule) {
        unsigned long minutes = seconds_until_refresh / 60;
        unsigned long seconds = seconds_until_refresh % 60;
        snprintf(refresh_buffer, sizeof(refresh_buffer), "Refresh %02lu:%02lu", minutes, seconds);
    } else {
        snprintf(refresh_buffer, sizeof(refresh_buffer), "Refresh --:--");
    }
    lv_label_set_text(refresh_label, refresh_buffer);
    request_full_redraw();
}

void ui_update_wifi_status(bool connected) {
    if (wifi_status_label == nullptr) {
        return;
    }

    lv_label_set_text(wifi_status_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(
        wifi_status_label,
        lv_color_hex(connected ? 0x78D08B : 0xF07B72),
        0
    );
}

void ui_update_page(const char* title, uint8_t page_index, uint8_t page_count) {
    apply_page_theme(page_index);

    if (station_label != nullptr) {
        lv_label_set_text(station_label, title == nullptr ? "" : title);
    }

    if (page_indicator_label != nullptr) {
        char buffer[16];
        uint8_t safeCount = page_count == 0 ? 1 : page_count;
        uint8_t safeIndex = page_index >= safeCount ? 0 : page_index;
        snprintf(buffer, sizeof(buffer), "%u/%u", static_cast<unsigned>(safeIndex + 1), static_cast<unsigned>(safeCount));
        lv_label_set_text(page_indicator_label, buffer);
    }

    request_full_redraw();
}

void ui_show_portal_page(const char* ssid, const char* url) {
    ensure_portal_page_overlay();
    lv_obj_clear_flag(portal_page_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(portal_page_overlay);

    String ssidLabel = "SSID: ";
    ssidLabel += (ssid == nullptr || strlen(ssid) == 0) ? "-" : ssid;
    lv_label_set_text(portal_page_ssid_label, ssidLabel.c_str());
    lv_label_set_text(portal_page_url_label, url == nullptr ? "http://-" : url);

    const char* qrPayload = (url == nullptr || strlen(url) == 0) ? "http://-" : url;
    lv_qrcode_update(portal_page_qr, qrPayload, strlen(qrPayload));

    request_full_redraw();
}

void ui_hide_portal_page() {
    if (portal_page_overlay == nullptr) {
        return;
    }

    lv_obj_add_flag(portal_page_overlay, LV_OBJ_FLAG_HIDDEN);
    request_full_redraw();
}

void ui_show_loading() {
    lv_label_set_text(status_label, "o");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xF3C969), 0);
    ui_update_clock("--:--:--", "-- --- ----", 0, false);
    request_full_redraw();
}

void ui_show_error(const char* message) {
    LV_UNUSED(message);
    lv_label_set_text(status_label, "!");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xF07B72), 0);
    ui_update_clock("--:--:--", "-- --- ----", 0, false);
    request_full_redraw();
}

void ui_show_setup(const char* ap_ssid, const char* ap_password, const char* url) {
    LV_UNUSED(ap_password);

    ensure_setup_overlay();
    lv_obj_clear_flag(setup_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(setup_overlay);

    String wifiLabel = "SSID: ";
    wifiLabel += ap_ssid;
    wifiLabel += "\nOpen network";
    lv_label_set_text(setup_wifi_label, wifiLabel.c_str());
    lv_label_set_text(setup_url_label, url);

    String wifiPayload = "WIFI:T:nopass;S:";
    wifiPayload += ap_ssid;
    wifiPayload += ";;";
    lv_qrcode_update(setup_wifi_qr, wifiPayload.c_str(), wifiPayload.length());
    lv_qrcode_update(setup_url_qr, url, strlen(url));

    request_full_redraw();
}

void ui_hide_setup() {
    if (setup_overlay == nullptr) {
        return;
    }

    lv_obj_add_flag(setup_overlay, LV_OBJ_FLAG_HIDDEN);
    if (portal_page_overlay != nullptr && !lv_obj_has_flag(portal_page_overlay, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_move_foreground(portal_page_overlay);
    }
    request_full_redraw();
}
