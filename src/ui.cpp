#include "ui.h"
#include "config.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <time.h>
#include <lvgl.h>
#include <misc/cache/instance/lv_image_cache.h>

extern Arduino_ESP32RGBPanel* displayBus;
extern Arduino_ST7701_RGBPanel* panel;
extern Arduino_GFX* gfx;
extern lv_disp_t* disp;
extern lv_indev_t* indev;

// Instrument-cluster palette: everything sits on black, colour is used only for
// outlined state tags, trend traces and the second hand.
static constexpr uint32_t COLOR_BG = 0x000000;
static constexpr uint32_t COLOR_INK = 0xF2F2F2;
static constexpr uint32_t COLOR_MUTED = 0x9A9A9A;
static constexpr uint32_t COLOR_FAINT = 0x5C5C5C;
static constexpr uint32_t COLOR_RULE = 0x1F1F1F;
static constexpr uint32_t COLOR_BEZEL = 0x707070;
static constexpr uint32_t COLOR_DIAL = 0x242424;
static constexpr uint32_t COLOR_HAND = 0xE8E8E8;
static constexpr uint32_t COLOR_ALERT = 0xE1372F;
static constexpr uint32_t COLOR_OK = 0x35D07F;
static constexpr uint32_t COLOR_WARN = 0xE8C23A;

static lv_obj_t* temp_label;
static lv_obj_t* humidity_label;
static lv_obj_t* pressure_label;
static lv_obj_t* wind_label;
static lv_obj_t* wind_readout_label;
static lv_obj_t* rain_label;
static lv_obj_t* sunrise_value_label;
static lv_obj_t* sunset_value_label;
static lv_obj_t* moon_phase_label;
static lv_obj_t* moon_canvas;
// The clock is drawn one character per fixed-width slot. Montserrat's digits
// are proportional ('1' is roughly half the width of '4'), so drawing the time
// as a single string makes it shuffle sideways every second.
static constexpr int TIME_SLOT_COUNT = 8;
static constexpr lv_coord_t TIME_DIGIT_WIDTH = 14;
static constexpr lv_coord_t TIME_COLON_WIDTH = 6;
static constexpr lv_coord_t TIME_RIGHT_EDGE = 310;
static lv_obj_t* time_slots[TIME_SLOT_COUNT];
static lv_obj_t* date_label;
static lv_obj_t* refresh_label;
static lv_obj_t* wifi_status_label;
static lv_obj_t* page_indicator_label;
static lv_obj_t* portal_page_overlay;
static constexpr int TRAFFIC_LIST_ROWS = 4;
static lv_obj_t* traffic_overlay;
static lv_obj_t* traffic_page_indicator;
static lv_obj_t* traffic_range_label;
static lv_obj_t* traffic_logo_image;
static lv_obj_t* traffic_callsign_label;
static lv_obj_t* traffic_airline_label;
static lv_obj_t* traffic_aircraft_label;
static lv_obj_t* traffic_route_label;
static lv_obj_t* traffic_route_city_label;
static lv_obj_t* traffic_metric_labels[4];
static lv_obj_t* traffic_row_callsign[TRAFFIC_LIST_ROWS];
static lv_obj_t* traffic_row_airline[TRAFFIC_LIST_ROWS];
static lv_obj_t* traffic_row_alt[TRAFFIC_LIST_ROWS];
static lv_obj_t* traffic_row_speed[TRAFFIC_LIST_ROWS];
static lv_obj_t* traffic_row_track[TRAFFIC_LIST_ROWS];
static lv_obj_t* traffic_footer_label;
static lv_image_dsc_t traffic_logo_dsc;
static char traffic_page_text[16];
static lv_obj_t* portal_page_ssid_label;
static lv_obj_t* portal_page_url_label;
static lv_obj_t* portal_page_qr;
static lv_obj_t* setup_overlay;
static lv_obj_t* setup_wifi_label;
static lv_obj_t* setup_url_label;
static lv_obj_t* setup_wifi_qr;
static lv_obj_t* setup_url_qr;
static constexpr int MOON_CANVAS_SIZE = 48;
static uint8_t moon_canvas_buffer[LV_CANVAS_BUF_SIZE(MOON_CANVAS_SIZE, MOON_CANVAS_SIZE, 32, LV_DRAW_BUF_STRIDE_ALIGN)];

static lv_style_t tag_text_style;
static lv_style_t micro_style;
static lv_style_t hero_value_style;
static lv_style_t metric_value_style;
static lv_style_t compact_value_style;
static lv_style_t time_style;
static lv_style_t date_style;
static lv_style_t refresh_style;
static lv_obj_t* screen_bg;

struct TagWidget {
    lv_obj_t* box;
    lv_obj_t* label;
};

static TagWidget station_tag;
static TagWidget status_tag;
static TagWidget temp_tag;
static TagWidget wind_tag;
static TagWidget humidity_tag;
static TagWidget rain_tag;
static TagWidget traffic_title_tag;
static TagWidget traffic_operator_tag;

static constexpr int CLOCK_SIZE = 140;
static constexpr int CLOCK_CENTER = CLOCK_SIZE / 2;
static lv_obj_t* clock_group;
static lv_obj_t* clock_hour_hand;
static lv_obj_t* clock_minute_hand;
static lv_obj_t* clock_second_hand;
static lv_obj_t* clock_hub;
static lv_point_precise_t clock_hour_points[2];
static lv_point_precise_t clock_minute_points[2];
static lv_point_precise_t clock_second_points[2];

static constexpr int WIND_DIAL_SIZE = 48;
static constexpr int WIND_DIAL_CENTER = WIND_DIAL_SIZE / 2;
static lv_obj_t* wind_dial;
static lv_obj_t* wind_needle;
static lv_point_precise_t wind_needle_points[2];

static constexpr uint8_t TREND_POINTS = 24;
static constexpr uint8_t TREND_PAGES = 5;

struct TrendSeries {
    int32_t values[TREND_POINTS];
    uint8_t count;
};

static TrendSeries temp_trend[TREND_PAGES];
static TrendSeries pressure_trend[TREND_PAGES];
static uint8_t active_trend_page;
static lv_obj_t* temp_chart;
static lv_obj_t* pressure_chart;
static lv_chart_series_t* temp_chart_series;
static lv_chart_series_t* pressure_chart_series;

static constexpr uint32_t DISPLAY_DRAW_BUFFER_LINES = 32;
// LVGL asserts (and its assert handler spins forever) unless the draw buffer
// meets its alignment requirement, which a plain uint16_t array does not
// guarantee once the surrounding statics shift around.
alignas(64) static uint16_t lvgl_draw_buffer[DISPLAY_WIDTH * DISPLAY_DRAW_BUFFER_LINES];

struct PageTheme {
    uint32_t primary;
    uint32_t secondary;
    uint32_t tertiary;
};

static constexpr PageTheme PAGE_THEMES[] = {
    {0x35D07F, 0xE8C23A, 0x4FC3F7},
    {0x4FC3F7, 0x7C9CF5, 0x35D07F},
    {0xE8C23A, 0xF2884B, 0xE05A7D},
    {0xB388FF, 0x4FC3F7, 0x35D07F}
};

static void draw_moon_phase(float phase);
static const char* moon_phase_name(float phase);
static void request_full_redraw();
static void configure_panel_variant();
static void apply_page_theme(uint8_t page_index);
static void ensure_portal_page_overlay();
static void ensure_setup_overlay();
static void ensure_traffic_overlay();

static void to_upper_copy(const char* src, char* dst, size_t dst_size, size_t max_chars) {
    if (dst == nullptr || dst_size == 0) {
        return;
    }

    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }

    size_t limit = dst_size - 1;
    if (max_chars > 0 && max_chars < limit) {
        limit = max_chars;
    }

    size_t i = 0;
    while (src[i] != '\0' && i < limit) {
        dst[i] = static_cast<char>(toupper(static_cast<unsigned char>(src[i])));
        ++i;
    }
    dst[i] = '\0';

    if (src[i] != '\0' && i >= 3) {
        dst[i - 3] = '.';
        dst[i - 2] = '.';
        dst[i - 1] = '.';
    }
}

// Local styles outrank both added and theme styles, so this leaves no way for a
// theme colour or a stray gradient to show through behind the dashboard.
static void force_black_background(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_bg_grad_color(obj, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_image_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static lv_obj_t* create_rule(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, uint32_t color) {
    lv_obj_t* rule = lv_obj_create(parent);
    lv_obj_remove_style_all(rule);
    lv_obj_set_size(rule, w, 1);
    lv_obj_set_pos(rule, x, y);
    lv_obj_set_style_bg_color(rule, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    return rule;
}

static lv_obj_t* create_micro(lv_obj_t* parent, const char* text, lv_coord_t x, lv_coord_t y) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_add_style(label, &micro_style, 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

static TagWidget create_tag(lv_obj_t* parent, const char* text, lv_coord_t x, lv_coord_t y, uint32_t color) {
    TagWidget tag;

    tag.box = lv_obj_create(parent);
    lv_obj_remove_style_all(tag.box);
    lv_obj_set_style_bg_opa(tag.box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tag.box, 1, 0);
    lv_obj_set_style_border_color(tag.box, lv_color_hex(color), 0);
    lv_obj_set_style_border_opa(tag.box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tag.box, 3, 0);
    lv_obj_set_style_pad_hor(tag.box, 8, 0);
    lv_obj_set_style_pad_ver(tag.box, 3, 0);
    lv_obj_set_size(tag.box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(tag.box, x, y);
    lv_obj_clear_flag(tag.box, LV_OBJ_FLAG_SCROLLABLE);

    tag.label = lv_label_create(tag.box);
    lv_label_set_text(tag.label, text);
    lv_obj_add_style(tag.label, &tag_text_style, 0);
    lv_obj_set_style_text_color(tag.label, lv_color_hex(color), 0);
    lv_obj_center(tag.label);

    return tag;
}

static void set_tag_color(const TagWidget& tag, uint32_t color) {
    if (tag.box == nullptr || tag.label == nullptr) {
        return;
    }

    lv_obj_set_style_border_color(tag.box, lv_color_hex(color), 0);
    lv_obj_set_style_text_color(tag.label, lv_color_hex(color), 0);
}

static void set_tag_text(const TagWidget& tag, const char* text) {
    if (tag.label == nullptr) {
        return;
    }

    lv_label_set_text(tag.label, text == nullptr ? "" : text);
}

static void align_tag_top_right(const TagWidget& tag, lv_coord_t x_offset, lv_coord_t y_offset) {
    if (tag.box == nullptr) {
        return;
    }

    lv_obj_align(tag.box, LV_ALIGN_TOP_RIGHT, x_offset, y_offset);
}

static lv_obj_t* create_hand(lv_obj_t* parent, lv_coord_t size, lv_coord_t width, uint32_t color) {
    lv_obj_t* hand = lv_line_create(parent);
    lv_obj_remove_style_all(hand);
    lv_obj_set_size(hand, size, size);
    lv_obj_set_pos(hand, 0, 0);
    lv_obj_set_style_line_width(hand, width, 0);
    lv_obj_set_style_line_color(hand, lv_color_hex(color), 0);
    lv_obj_set_style_line_opa(hand, LV_OPA_COVER, 0);
    lv_obj_set_style_line_rounded(hand, true, 0);
    return hand;
}

static void set_hand_points(lv_obj_t* hand, lv_point_precise_t* points, int center, float degrees, int length, int tail) {
    if (hand == nullptr) {
        return;
    }

    const float radians = degrees * PI / 180.0f;
    const float dx = sinf(radians);
    const float dy = -cosf(radians);

    points[0].x = static_cast<lv_value_precise_t>(center - dx * tail);
    points[0].y = static_cast<lv_value_precise_t>(center - dy * tail);
    points[1].x = static_cast<lv_value_precise_t>(center + dx * length);
    points[1].y = static_cast<lv_value_precise_t>(center + dy * length);

    lv_line_set_points(hand, points, 2);
}

static void create_clock(lv_obj_t* parent, lv_coord_t x, lv_coord_t y) {
    clock_group = lv_obj_create(parent);
    lv_obj_remove_style_all(clock_group);
    lv_obj_set_size(clock_group, CLOCK_SIZE, CLOCK_SIZE);
    lv_obj_set_pos(clock_group, x, y);
    lv_obj_clear_flag(clock_group, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* bezel = lv_obj_create(clock_group);
    lv_obj_remove_style_all(bezel);
    lv_obj_set_size(bezel, CLOCK_SIZE, CLOCK_SIZE);
    lv_obj_set_pos(bezel, 0, 0);
    lv_obj_set_style_radius(bezel, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bezel, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(bezel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bezel, 2, 0);
    lv_obj_set_style_border_color(bezel, lv_color_hex(COLOR_BEZEL), 0);
    lv_obj_set_style_border_opa(bezel, LV_OPA_COVER, 0);

    lv_obj_t* inner_ring = lv_obj_create(clock_group);
    lv_obj_remove_style_all(inner_ring);
    lv_obj_set_size(inner_ring, CLOCK_SIZE - 16, CLOCK_SIZE - 16);
    lv_obj_align(inner_ring, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(inner_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(inner_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(inner_ring, 1, 0);
    lv_obj_set_style_border_color(inner_ring, lv_color_hex(COLOR_DIAL), 0);
    lv_obj_set_style_border_opa(inner_ring, LV_OPA_COVER, 0);

    for (int hour = 1; hour <= 12; ++hour) {
        const float radians = hour * 30.0f * PI / 180.0f;
        const int radius = 52;

        char text[3];
        snprintf(text, sizeof(text), "%d", hour);

        lv_obj_t* numeral = lv_label_create(clock_group);
        lv_label_set_text(numeral, text);
        lv_obj_set_style_text_font(numeral, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(numeral, lv_color_hex(hour % 3 == 0 ? COLOR_INK : COLOR_MUTED), 0);
        lv_obj_align(
            numeral,
            LV_ALIGN_CENTER,
            static_cast<lv_coord_t>(sinf(radians) * radius),
            static_cast<lv_coord_t>(-cosf(radians) * radius)
        );
    }

    clock_hour_hand = create_hand(clock_group, CLOCK_SIZE, 4, COLOR_HAND);
    clock_minute_hand = create_hand(clock_group, CLOCK_SIZE, 3, COLOR_HAND);
    clock_second_hand = create_hand(clock_group, CLOCK_SIZE, 2, COLOR_ALERT);

    clock_hub = lv_obj_create(clock_group);
    lv_obj_remove_style_all(clock_hub);
    lv_obj_set_size(clock_hub, 9, 9);
    lv_obj_align(clock_hub, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(clock_hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(clock_hub, lv_color_hex(COLOR_ALERT), 0);
    lv_obj_set_style_bg_opa(clock_hub, LV_OPA_COVER, 0);

    set_hand_points(clock_hour_hand, clock_hour_points, CLOCK_CENTER, 0.0f, 34, 0);
    set_hand_points(clock_minute_hand, clock_minute_points, CLOCK_CENTER, 0.0f, 50, 0);
    set_hand_points(clock_second_hand, clock_second_points, CLOCK_CENTER, 0.0f, 56, 14);
}

static void update_clock_hands(int hours, int minutes, int seconds) {
    const float minute_fraction = minutes + (seconds / 60.0f);
    const float hour_angle = ((hours % 12) + (minute_fraction / 60.0f)) * 30.0f;
    const float minute_angle = minute_fraction * 6.0f;
    const float second_angle = seconds * 6.0f;

    set_hand_points(clock_hour_hand, clock_hour_points, CLOCK_CENTER, hour_angle, 34, 0);
    set_hand_points(clock_minute_hand, clock_minute_points, CLOCK_CENTER, minute_angle, 50, 0);
    set_hand_points(clock_second_hand, clock_second_points, CLOCK_CENTER, second_angle, 56, 14);
}

static void create_wind_dial(lv_obj_t* parent, lv_coord_t x, lv_coord_t y) {
    wind_dial = lv_obj_create(parent);
    lv_obj_remove_style_all(wind_dial);
    lv_obj_set_size(wind_dial, WIND_DIAL_SIZE, WIND_DIAL_SIZE);
    lv_obj_set_pos(wind_dial, x, y);
    lv_obj_set_style_radius(wind_dial, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(wind_dial, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wind_dial, 1, 0);
    lv_obj_set_style_border_color(wind_dial, lv_color_hex(COLOR_DIAL), 0);
    lv_obj_set_style_border_opa(wind_dial, LV_OPA_COVER, 0);
    lv_obj_clear_flag(wind_dial, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* north_mark = lv_obj_create(wind_dial);
    lv_obj_remove_style_all(north_mark);
    lv_obj_set_size(north_mark, 2, 5);
    lv_obj_align(north_mark, LV_ALIGN_TOP_MID, 0, 3);
    lv_obj_set_style_bg_color(north_mark, lv_color_hex(COLOR_FAINT), 0);
    lv_obj_set_style_bg_opa(north_mark, LV_OPA_COVER, 0);

    wind_needle = create_hand(wind_dial, WIND_DIAL_SIZE - 2, 2, COLOR_INK);
    set_hand_points(wind_needle, wind_needle_points, WIND_DIAL_CENTER - 1, 0.0f, 15, 8);
}

static void update_wind_direction_icon(float degrees) {
    while (degrees < 0.0f) {
        degrees += 360.0f;
    }
    while (degrees >= 360.0f) {
        degrees -= 360.0f;
    }

    set_hand_points(wind_needle, wind_needle_points, WIND_DIAL_CENTER - 1, degrees, 15, 8);
}

static lv_obj_t* create_trend_chart(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, uint32_t color, lv_chart_series_t** series) {
    lv_obj_t* chart = lv_chart_create(parent);
    lv_obj_set_pos(chart, x, y);
    lv_obj_set_size(chart, w, h);
    lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_set_style_pad_all(chart, 0, 0);
    lv_obj_set_style_radius(chart, 0, 0);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_clear_flag(chart, LV_OBJ_FLAG_SCROLLABLE);

    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(chart, 0, 0);
    lv_chart_set_point_count(chart, TREND_POINTS);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);

    *series = lv_chart_add_series(chart, lv_color_hex(color), LV_CHART_AXIS_PRIMARY_Y);
    return chart;
}

static void trend_push(TrendSeries& trend, int32_t value) {
    if (trend.count < TREND_POINTS) {
        trend.values[trend.count] = value;
        ++trend.count;
        return;
    }

    for (uint8_t i = 1; i < TREND_POINTS; ++i) {
        trend.values[i - 1] = trend.values[i];
    }
    trend.values[TREND_POINTS - 1] = value;
}

static void trend_render(lv_obj_t* chart, lv_chart_series_t* series, const TrendSeries& trend, int32_t min_padding) {
    if (chart == nullptr || series == nullptr) {
        return;
    }

    int32_t lowest = 0;
    int32_t highest = 0;
    if (trend.count > 0) {
        lowest = trend.values[0];
        highest = trend.values[0];
        for (uint8_t i = 1; i < trend.count; ++i) {
            if (trend.values[i] < lowest) {
                lowest = trend.values[i];
            }
            if (trend.values[i] > highest) {
                highest = trend.values[i];
            }
        }
    }

    int32_t padding = (highest - lowest) / 8;
    if (padding < min_padding) {
        padding = min_padding;
    }
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, lowest - padding, highest + padding);

    // The newest sample always sits on the right edge; unused slots stay blank.
    const uint8_t offset = TREND_POINTS - trend.count;
    for (uint8_t i = 0; i < TREND_POINTS; ++i) {
        const int32_t value = (i < offset) ? LV_CHART_POINT_NONE : trend.values[i - offset];
        lv_chart_set_series_value_by_id(chart, series, i, value);
    }
}

static void render_active_trends() {
    trend_render(temp_chart, temp_chart_series, temp_trend[active_trend_page], 5);
    trend_render(pressure_chart, pressure_chart_series, pressure_trend[active_trend_page], 2);
}

static void apply_page_theme(uint8_t page_index) {
    const PageTheme& theme = PAGE_THEMES[page_index % (sizeof(PAGE_THEMES) / sizeof(PAGE_THEMES[0]))];

    set_tag_color(station_tag, theme.primary);
    set_tag_color(temp_tag, theme.primary);
    set_tag_color(wind_tag, theme.secondary);
    set_tag_color(humidity_tag, theme.tertiary);
    set_tag_color(rain_tag, theme.secondary);

    if (temp_chart != nullptr && temp_chart_series != nullptr) {
        lv_chart_set_series_color(temp_chart, temp_chart_series, lv_color_hex(theme.primary));
    }
    if (pressure_chart != nullptr && pressure_chart_series != nullptr) {
        lv_chart_set_series_color(pressure_chart, pressure_chart_series, lv_color_hex(theme.tertiary));
    }
}

static void set_status(const char* text, uint32_t color) {
    set_tag_text(status_tag, text);
    set_tag_color(status_tag, color);
    align_tag_top_right(status_tag, -104, 10);
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
        // The panel is wired BGR: red and blue came out swapped (a red accent
        // rendered blue), so the blue pins drive the red channel and vice versa.
        DISPLAY_PIN_B0,
        DISPLAY_PIN_B1,
        DISPLAY_PIN_B2,
        DISPLAY_PIN_B3,
        DISPLAY_PIN_B4,
        DISPLAY_PIN_G0,
        DISPLAY_PIN_G1,
        DISPLAY_PIN_G2,
        DISPLAY_PIN_G3,
        DISPLAY_PIN_G4,
        DISPLAY_PIN_G5,
        DISPLAY_PIN_R0,
        DISPLAY_PIN_R1,
        DISPLAY_PIN_R2,
        DISPLAY_PIN_R3,
        DISPLAY_PIN_R4,
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
    const lv_color_t lit = lv_color_hex(0xE8E8E8);
    const lv_color_t shadow = lv_color_hex(0x1E1E1E);

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
        return "NEW MOON";
    }
    if (phase < 0.1875f) {
        return "WAXING CRESCENT";
    }
    if (phase < 0.3125f) {
        return "FIRST QUARTER";
    }
    if (phase < 0.4375f) {
        return "WAXING GIBBOUS";
    }
    if (phase < 0.5625f) {
        return "FULL MOON";
    }
    if (phase < 0.6875f) {
        return "WANING GIBBOUS";
    }
    if (phase < 0.8125f) {
        return "THIRD QUARTER";
    }
    return "WANING CRESCENT";
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
    // Arduino_ST7701_RGBPanel::begin() sends INVON for IPS panels, but this one
    // renders inverted with it: black came out white. Turn inversion back off
    // while the controller is still on the standard command page.
    displayBus->beginWrite();
    displayBus->writeCommand(0x20); // INVOFF
    displayBus->endWrite();

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

// Updating a page calls several ui_* functions, and repainting inside each one
// cost a full 480x480 redraw apiece: a page switch spent ~800ms doing the same
// work five times. They now mark the screen dirty and the main loop repaints
// once per iteration.
static bool redraw_pending = false;

static void request_full_redraw() {
    redraw_pending = true;
}

void ui_flush() {
    if (!redraw_pending || disp == nullptr) {
        return;
    }

    redraw_pending = false;
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(disp);
}

static void style_overlay_panel(lv_obj_t* panel_box) {
    force_black_background(panel_box);
    lv_obj_set_style_border_color(panel_box, lv_color_hex(COLOR_RULE), 0);
    lv_obj_set_style_border_width(panel_box, 1, 0);
    lv_obj_set_style_radius(panel_box, 6, 0);
    lv_obj_clear_flag(panel_box, LV_OBJ_FLAG_SCROLLABLE);
}

static void style_overlay_qr(lv_obj_t* qr) {
    lv_qrcode_set_dark_color(qr, lv_color_hex(0x000000));
    lv_qrcode_set_light_color(qr, lv_color_hex(0xF2F2F2));
}

static void ensure_traffic_overlay() {
    if (traffic_overlay != nullptr) {
        return;
    }

    traffic_overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(traffic_overlay);
    lv_obj_set_size(traffic_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    force_black_background(traffic_overlay);
    lv_obj_clear_flag(traffic_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(traffic_overlay, LV_OBJ_FLAG_HIDDEN);

    traffic_title_tag = create_tag(traffic_overlay, "AIR TRAFFIC", 18, 10, 0x4FC3F7);

    traffic_page_indicator = lv_label_create(traffic_overlay);
    lv_label_set_text(traffic_page_indicator, "");
    lv_obj_add_style(traffic_page_indicator, &micro_style, 0);
    lv_obj_align(traffic_page_indicator, LV_ALIGN_TOP_RIGHT, -18, 14);

    traffic_range_label = lv_label_create(traffic_overlay);
    lv_label_set_text(traffic_range_label, "");
    lv_obj_add_style(traffic_range_label, &micro_style, 0);
    lv_obj_align(traffic_range_label, LV_ALIGN_TOP_RIGHT, -58, 14);

    create_rule(traffic_overlay, 18, 44, 444, COLOR_RULE);

    // Featured aircraft: the closest one in range.
    traffic_logo_image = lv_image_create(traffic_overlay);
    lv_obj_set_pos(traffic_logo_image, 18, 58);
    lv_obj_add_flag(traffic_logo_image, LV_OBJ_FLAG_HIDDEN);

    traffic_operator_tag = create_tag(traffic_overlay, "---", 18, 74, 0x4FC3F7);
    lv_obj_add_flag(traffic_operator_tag.box, LV_OBJ_FLAG_HIDDEN);

    traffic_callsign_label = lv_label_create(traffic_overlay);
    lv_label_set_text(traffic_callsign_label, "NO CONTACTS");
    lv_obj_add_style(traffic_callsign_label, &metric_value_style, 0);
    lv_obj_set_pos(traffic_callsign_label, 96, 56);

    traffic_airline_label = lv_label_create(traffic_overlay);
    lv_label_set_text(traffic_airline_label, "");
    lv_obj_add_style(traffic_airline_label, &date_style, 0);
    lv_obj_set_style_text_align(traffic_airline_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(traffic_airline_label, 250);
    lv_label_set_long_mode(traffic_airline_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(traffic_airline_label, 96, 100);

    traffic_aircraft_label = lv_label_create(traffic_overlay);
    lv_label_set_text(traffic_aircraft_label, "");
    lv_obj_add_style(traffic_aircraft_label, &micro_style, 0);
    lv_obj_set_style_text_align(traffic_aircraft_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(traffic_aircraft_label, LV_ALIGN_TOP_RIGHT, -18, 105);

    traffic_route_label = lv_label_create(traffic_overlay);
    lv_label_set_text(traffic_route_label, "");
    lv_obj_add_style(traffic_route_label, &compact_value_style, 0);
    lv_obj_set_pos(traffic_route_label, 96, 124);

    traffic_route_city_label = lv_label_create(traffic_overlay);
    lv_label_set_text(traffic_route_city_label, "");
    lv_obj_add_style(traffic_route_city_label, &micro_style, 0);
    lv_obj_set_style_text_align(traffic_route_city_label, LV_TEXT_ALIGN_RIGHT, 0);
    // Bounded so a long pair of city names cannot run back into the airport
    // codes on the left.
    lv_obj_set_width(traffic_route_city_label, 250);
    lv_label_set_long_mode(traffic_route_city_label, LV_LABEL_LONG_DOT);
    lv_obj_align(traffic_route_city_label, LV_ALIGN_TOP_RIGHT, -18, 130);

    static const char* METRIC_CAPTIONS[] = {"ALT ft", "SPEED kt", "RANGE nm", "TRACK"};
    static const lv_coord_t METRIC_X[] = {18, 132, 246, 360};
    for (int i = 0; i < 4; ++i) {
        create_micro(traffic_overlay, METRIC_CAPTIONS[i], METRIC_X[i], 156);
        traffic_metric_labels[i] = lv_label_create(traffic_overlay);
        lv_label_set_text(traffic_metric_labels[i], "--");
        lv_obj_add_style(traffic_metric_labels[i], &compact_value_style, 0);
        lv_obj_set_pos(traffic_metric_labels[i], METRIC_X[i], 176);
    }

    create_rule(traffic_overlay, 18, 214, 444, COLOR_RULE);
    create_micro(traffic_overlay, "ALSO IN RANGE", 18, 224);

    for (int row = 0; row < TRAFFIC_LIST_ROWS; ++row) {
        const lv_coord_t y = 250 + (row * 38);

        traffic_row_callsign[row] = lv_label_create(traffic_overlay);
        lv_label_set_text(traffic_row_callsign[row], "");
        lv_obj_add_style(traffic_row_callsign[row], &compact_value_style, 0);
        lv_obj_set_pos(traffic_row_callsign[row], 18, y);

        traffic_row_airline[row] = lv_label_create(traffic_overlay);
        lv_label_set_text(traffic_row_airline[row], "");
        lv_obj_add_style(traffic_row_airline[row], &micro_style, 0);
        lv_obj_set_style_text_color(traffic_row_airline[row], lv_color_hex(COLOR_MUTED), 0);
        lv_obj_set_width(traffic_row_airline[row], 92);
        lv_label_set_long_mode(traffic_row_airline[row], LV_LABEL_LONG_DOT);
        lv_obj_set_pos(traffic_row_airline[row], 150, y + 5);

        // Right-aligned columns rather than one string, so altitude, speed and
        // track line up down the list.
        lv_obj_t** columns[] = {&traffic_row_alt[row], &traffic_row_speed[row], &traffic_row_track[row]};
        static const lv_coord_t COLUMN_RIGHT[] = {-132, -72, -18};
        for (int column = 0; column < 3; ++column) {
            *columns[column] = lv_label_create(traffic_overlay);
            lv_label_set_text(*columns[column], "");
            lv_obj_add_style(*columns[column], &micro_style, 0);
            lv_obj_set_style_text_align(*columns[column], LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(*columns[column], LV_ALIGN_TOP_RIGHT, COLUMN_RIGHT[column], y + 5);
        }
    }

    create_rule(traffic_overlay, 18, 424, 444, COLOR_RULE);

    traffic_footer_label = lv_label_create(traffic_overlay);
    lv_label_set_text(traffic_footer_label, "");
    lv_obj_add_style(traffic_footer_label, &micro_style, 0);
    lv_obj_set_pos(traffic_footer_label, 18, 438);
}

static void ensure_portal_page_overlay() {
    if (portal_page_overlay != nullptr) {
        return;
    }

    portal_page_overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(portal_page_overlay);
    lv_obj_set_size(portal_page_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    force_black_background(portal_page_overlay);
    lv_obj_clear_flag(portal_page_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(portal_page_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* panel_box = lv_obj_create(portal_page_overlay);
    lv_obj_set_size(panel_box, 464, 464);
    lv_obj_center(panel_box);
    style_overlay_panel(panel_box);

    create_tag(panel_box, "SETUP PAGE", 24, 22, COLOR_OK);

    lv_obj_t* subtitle = lv_label_create(panel_box);
    lv_label_set_text(subtitle, "Scan to open the setup page while your phone is on the same Wi-Fi network.");
    lv_obj_set_width(subtitle, 408);
    lv_label_set_long_mode(subtitle, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 24, 62);

    create_rule(panel_box, 24, 122, 416, COLOR_RULE);

    lv_obj_t* network_title = create_micro(panel_box, "CONNECTED NETWORK", 24, 140);
    LV_UNUSED(network_title);

    portal_page_ssid_label = lv_label_create(panel_box);
    lv_label_set_text(portal_page_ssid_label, "SSID");
    lv_obj_set_width(portal_page_ssid_label, 180);
    lv_label_set_long_mode(portal_page_ssid_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(portal_page_ssid_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(portal_page_ssid_label, lv_color_hex(COLOR_INK), 0);
    lv_obj_align(portal_page_ssid_label, LV_ALIGN_TOP_LEFT, 24, 162);

    create_micro(panel_box, "SETUP URL", 24, 240);

    portal_page_url_label = lv_label_create(panel_box);
    lv_label_set_text(portal_page_url_label, "http://0.0.0.0");
    lv_obj_set_width(portal_page_url_label, 180);
    lv_label_set_long_mode(portal_page_url_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(portal_page_url_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(portal_page_url_label, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_align(portal_page_url_label, LV_ALIGN_TOP_LEFT, 24, 262);

    portal_page_qr = lv_qrcode_create(panel_box);
    lv_qrcode_set_size(portal_page_qr, 172);
    style_overlay_qr(portal_page_qr);
    lv_obj_align(portal_page_qr, LV_ALIGN_TOP_RIGHT, -34, 150);

    create_rule(panel_box, 24, 400, 416, COLOR_RULE);

    lv_obj_t* footer = create_micro(panel_box, "SWIPE RIGHT TO RETURN", 24, 420);
    LV_UNUSED(footer);
}

static void ensure_setup_overlay() {
    if (setup_overlay != nullptr) {
        return;
    }

    setup_overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(setup_overlay);
    lv_obj_set_size(setup_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    force_black_background(setup_overlay);
    lv_obj_clear_flag(setup_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* panel_box = lv_obj_create(setup_overlay);
    lv_obj_set_size(panel_box, 464, 464);
    lv_obj_center(panel_box);
    style_overlay_panel(panel_box);

    create_tag(panel_box, "SETUP REQUIRED", 24, 22, COLOR_ALERT);

    lv_obj_t* subtitle = lv_label_create(panel_box);
    lv_label_set_text(subtitle, "1. Join the setup Wi-Fi\n2. Scan the page QR code\n3. Save your settings");
    lv_obj_set_width(subtitle, 408);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 24, 62);

    create_rule(panel_box, 24, 140, 416, COLOR_RULE);

    create_micro(panel_box, "JOIN WI-FI", 24, 156);

    setup_wifi_label = lv_label_create(panel_box);
    lv_label_set_text(setup_wifi_label, "SSID");
    lv_obj_set_width(setup_wifi_label, 190);
    lv_label_set_long_mode(setup_wifi_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(setup_wifi_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(setup_wifi_label, lv_color_hex(COLOR_INK), 0);
    lv_obj_align(setup_wifi_label, LV_ALIGN_TOP_LEFT, 24, 178);

    setup_wifi_qr = lv_qrcode_create(panel_box);
    lv_qrcode_set_size(setup_wifi_qr, 150);
    style_overlay_qr(setup_wifi_qr);
    lv_obj_align(setup_wifi_qr, LV_ALIGN_TOP_LEFT, 24, 250);

    create_micro(panel_box, "OPEN SETUP PAGE", 254, 156);

    setup_url_label = lv_label_create(panel_box);
    lv_label_set_text(setup_url_label, "http://192.168.4.1");
    lv_obj_set_width(setup_url_label, 190);
    lv_label_set_long_mode(setup_url_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(setup_url_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(setup_url_label, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_align(setup_url_label, LV_ALIGN_TOP_LEFT, 254, 178);

    setup_url_qr = lv_qrcode_create(panel_box);
    lv_qrcode_set_size(setup_url_qr, 150);
    style_overlay_qr(setup_url_qr);
    lv_obj_align(setup_url_qr, LV_ALIGN_TOP_LEFT, 254, 250);
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

    lv_style_init(&tag_text_style);
    lv_style_set_text_font(&tag_text_style, &lv_font_montserrat_14);
    lv_style_set_text_letter_space(&tag_text_style, 1);

    lv_style_init(&micro_style);
    lv_style_set_text_font(&micro_style, &lv_font_montserrat_14);
    lv_style_set_text_color(&micro_style, lv_color_hex(COLOR_FAINT));
    lv_style_set_text_letter_space(&micro_style, 1);

    lv_style_init(&hero_value_style);
    lv_style_set_text_font(&hero_value_style, &lv_font_montserrat_48);
    lv_style_set_text_color(&hero_value_style, lv_color_hex(COLOR_INK));

    lv_style_init(&metric_value_style);
    lv_style_set_text_font(&metric_value_style, &lv_font_montserrat_36);
    lv_style_set_text_color(&metric_value_style, lv_color_hex(COLOR_INK));

    lv_style_init(&compact_value_style);
    lv_style_set_text_font(&compact_value_style, &lv_font_montserrat_20);
    lv_style_set_text_color(&compact_value_style, lv_color_hex(COLOR_INK));

    lv_style_init(&time_style);
    lv_style_set_text_font(&time_style, &lv_font_montserrat_20);
    lv_style_set_text_color(&time_style, lv_color_hex(COLOR_INK));
    lv_style_set_text_align(&time_style, LV_TEXT_ALIGN_RIGHT);

    lv_style_init(&date_style);
    lv_style_set_text_font(&date_style, &lv_font_montserrat_16);
    lv_style_set_text_color(&date_style, lv_color_hex(COLOR_MUTED));
    lv_style_set_text_align(&date_style, LV_TEXT_ALIGN_RIGHT);
    lv_style_set_text_letter_space(&date_style, 1);

    lv_style_init(&refresh_style);
    lv_style_set_text_font(&refresh_style, &lv_font_montserrat_14);
    lv_style_set_text_color(&refresh_style, lv_color_hex(COLOR_FAINT));
    lv_style_set_text_align(&refresh_style, LV_TEXT_ALIGN_RIGHT);
    lv_style_set_text_letter_space(&refresh_style, 1);

    lv_obj_t* screen = lv_scr_act();
    force_black_background(screen);

    screen_bg = lv_obj_create(screen);
    lv_obj_set_size(screen_bg, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_pos(screen_bg, 0, 0);
    force_black_background(screen_bg);
    lv_obj_clear_flag(screen_bg, LV_OBJ_FLAG_SCROLLABLE);

    // Header ------------------------------------------------------------
    station_tag = create_tag(screen_bg, "MARLOW WEATHER", 18, 10, 0x35D07F);

    status_tag = create_tag(screen_bg, "WAIT", 0, 0, COLOR_WARN);
    align_tag_top_right(status_tag, -104, 10);

    wifi_status_label = lv_label_create(screen_bg);
    lv_label_set_text(wifi_status_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(COLOR_WARN), 0);
    lv_obj_align(wifi_status_label, LV_ALIGN_TOP_RIGHT, -62, 14);

    page_indicator_label = lv_label_create(screen_bg);
    lv_label_set_text(page_indicator_label, "1/1");
    lv_obj_add_style(page_indicator_label, &micro_style, 0);
    lv_obj_align(page_indicator_label, LV_ALIGN_TOP_RIGHT, -18, 14);

    create_rule(screen_bg, 18, 44, 444, COLOR_RULE);

    // Primary readouts ---------------------------------------------------
    temp_tag = create_tag(screen_bg, "TEMP \xC2\xB0" "C", 18, 58, 0x35D07F);

    temp_label = lv_label_create(screen_bg);
    lv_label_set_text(temp_label, "--.-\xC2\xB0");
    lv_obj_add_style(temp_label, &hero_value_style, 0);
    lv_obj_set_pos(temp_label, 18, 86);

    wind_tag = create_tag(screen_bg, "WIND m/s", 18, 148, 0xE8C23A);

    wind_label = lv_label_create(screen_bg);
    lv_label_set_text(wind_label, "--.-");
    lv_obj_add_style(wind_label, &metric_value_style, 0);
    lv_obj_set_pos(wind_label, 18, 176);

    lv_coord_t slot_x = TIME_RIGHT_EDGE - ((6 * TIME_DIGIT_WIDTH) + (2 * TIME_COLON_WIDTH));
    for (int i = 0; i < TIME_SLOT_COUNT; ++i) {
        const bool is_colon = (i == 2) || (i == 5);
        const lv_coord_t slot_w = is_colon ? TIME_COLON_WIDTH : TIME_DIGIT_WIDTH;

        time_slots[i] = lv_label_create(screen_bg);
        lv_label_set_text(time_slots[i], is_colon ? ":" : "-");
        lv_obj_add_style(time_slots[i], &time_style, 0);
        lv_obj_set_style_text_align(time_slots[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(time_slots[i], slot_w);
        lv_obj_set_pos(time_slots[i], slot_x, 62);

        slot_x += slot_w;
    }

    date_label = lv_label_create(screen_bg);
    lv_label_set_text(date_label, "-- --- ----");
    lv_obj_add_style(date_label, &date_style, 0);
    lv_obj_align(date_label, LV_ALIGN_TOP_RIGHT, -170, 92);

    refresh_label = lv_label_create(screen_bg);
    lv_label_set_text(refresh_label, "REFRESH --:--");
    lv_obj_add_style(refresh_label, &refresh_style, 0);
    lv_obj_align(refresh_label, LV_ALIGN_TOP_RIGHT, -170, 118);

    create_clock(screen_bg, 322, 60);

    create_rule(screen_bg, 18, 226, 444, COLOR_RULE);

    // Trends and secondary metrics ---------------------------------------
    create_micro(screen_bg, "TEMPERATURE TREND", 18, 236);
    temp_chart = create_trend_chart(screen_bg, 18, 256, 282, 50, 0x35D07F, &temp_chart_series);
    create_rule(screen_bg, 18, 308, 282, COLOR_RULE);

    create_micro(screen_bg, "PRESSURE hPa", 18, 316);

    pressure_label = lv_label_create(screen_bg);
    lv_label_set_text(pressure_label, "---- =");
    lv_obj_add_style(pressure_label, &compact_value_style, 0);
    lv_obj_set_style_text_align(pressure_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(pressure_label, LV_ALIGN_TOP_RIGHT, -180, 310);

    // The section rule at y=400 doubles as this chart's baseline.
    pressure_chart = create_trend_chart(screen_bg, 18, 338, 282, 50, 0x4FC3F7, &pressure_chart_series);

    humidity_tag = create_tag(screen_bg, "HUMIDITY %", 320, 236, 0x4FC3F7);

    humidity_label = lv_label_create(screen_bg);
    lv_label_set_text(humidity_label, "--");
    lv_obj_add_style(humidity_label, &metric_value_style, 0);
    lv_obj_set_pos(humidity_label, 320, 262);

    rain_tag = create_tag(screen_bg, "RAIN mm", 320, 316, 0xE8C23A);

    rain_label = lv_label_create(screen_bg);
    lv_label_set_text(rain_label, "--.-");
    lv_obj_add_style(rain_label, &metric_value_style, 0);
    lv_obj_set_pos(rain_label, 320, 342);

    create_rule(screen_bg, 18, 400, 444, COLOR_RULE);

    // Footer strip --------------------------------------------------------
    create_micro(screen_bg, "RISE", 18, 410);

    sunrise_value_label = lv_label_create(screen_bg);
    lv_label_set_text(sunrise_value_label, "--:--");
    lv_obj_add_style(sunrise_value_label, &compact_value_style, 0);
    lv_obj_set_pos(sunrise_value_label, 18, 430);

    create_micro(screen_bg, "SET", 110, 410);

    sunset_value_label = lv_label_create(screen_bg);
    lv_label_set_text(sunset_value_label, "--:--");
    lv_obj_add_style(sunset_value_label, &compact_value_style, 0);
    lv_obj_set_pos(sunset_value_label, 110, 430);

    create_micro(screen_bg, "MOON", 202, 410);

    moon_canvas = lv_canvas_create(screen_bg);
    lv_obj_remove_style_all(moon_canvas);
    lv_canvas_set_buffer(moon_canvas, moon_canvas_buffer, MOON_CANVAS_SIZE, MOON_CANVAS_SIZE, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_pos(moon_canvas, 202, 428);
    draw_moon_phase(0.0f);

    moon_phase_label = lv_label_create(screen_bg);
    lv_label_set_text(moon_phase_label, "NEW MOON");
    lv_obj_add_style(moon_phase_label, &micro_style, 0);
    lv_obj_set_style_text_color(moon_phase_label, lv_color_hex(COLOR_MUTED), 0);
    // A size down from the other micro labels so the longest phase names
    // ("WAXING CRESCENT") always break into two lines rather than three.
    lv_obj_set_style_text_font(moon_phase_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_letter_space(moon_phase_label, 0, 0);
    lv_obj_set_width(moon_phase_label, 88);
    lv_label_set_long_mode(moon_phase_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(moon_phase_label, 256, 434);

    create_micro(screen_bg, "WIND DIR", 348, 410);
    create_wind_dial(screen_bg, 348, 428);

    wind_readout_label = lv_label_create(screen_bg);
    lv_label_set_text(wind_readout_label, "---\xC2\xB0 --");
    lv_obj_add_style(wind_readout_label, &micro_style, 0);
    lv_obj_set_style_text_color(wind_readout_label, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_pos(wind_readout_label, 404, 444);

    apply_page_theme(0);
    render_active_trends();

    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(disp);
}

void ui_update_weather(const WeatherData& data) {
    if (!data.isValid) {
        ui_show_error("No data available");
        return;
    }

    set_status("LIVE", COLOR_OK);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f\xC2\xB0", data.temperature);
    lv_label_set_text(temp_label, buffer);

    snprintf(buffer, sizeof(buffer), "%.0f", data.humidity);
    lv_label_set_text(humidity_label, buffer);

    snprintf(buffer, sizeof(buffer), "%.1f", data.windSpeed);
    lv_label_set_text(wind_label, buffer);

    const char* cardinal = wind_cardinal(data.windDirection);
    // Keep to ASCII: LVGL's built-in Montserrat has no middle dot glyph.
    snprintf(buffer, sizeof(buffer), "WIND m/s - %s", cardinal);
    set_tag_text(wind_tag, buffer);

    snprintf(buffer, sizeof(buffer), "%.0f\xC2\xB0 %s", data.windDirection, cardinal);
    lv_label_set_text(wind_readout_label, buffer);
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

    trend_push(temp_trend[active_trend_page], static_cast<int32_t>(data.temperature * 10.0f));
    trend_push(pressure_trend[active_trend_page], static_cast<int32_t>(data.pressure));
    render_active_trends();

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

// Spreads "HH:MM:SS" across the fixed-width slots, one character each.
static void set_time_text(const char* text) {
    if (text == nullptr) {
        return;
    }

    const size_t length = strlen(text);
    for (int i = 0; i < TIME_SLOT_COUNT; ++i) {
        if (time_slots[i] == nullptr) {
            continue;
        }

        const char character[2] = {
            static_cast<size_t>(i) < length ? text[i] : ' ',
            '\0'
        };
        if (strcmp(lv_label_get_text(time_slots[i]), character) != 0) {
            lv_label_set_text(time_slots[i], character);
        }
    }
}

void ui_update_clock(const char* time_text, const char* date_text, unsigned long seconds_until_refresh, bool has_refresh_schedule) {
    if (time_slots[0] == nullptr || date_label == nullptr || refresh_label == nullptr) {
        return;
    }

    if (time_text == nullptr || date_text == nullptr) {
        set_time_text("--:--:--");
        lv_label_set_text(date_label, "-- --- ----");
        lv_label_set_text(refresh_label, "REFRESH --:--");
        return;
    }

    set_time_text(time_text);

    char date_buffer[24];
    to_upper_copy(date_text, date_buffer, sizeof(date_buffer), 0);
    lv_label_set_text(date_label, date_buffer);

    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    if (sscanf(time_text, "%d:%d:%d", &hours, &minutes, &seconds) == 3) {
        update_clock_hands(hours, minutes, seconds);
    }

    char refresh_buffer[24];
    if (has_refresh_schedule) {
        unsigned long remaining_minutes = seconds_until_refresh / 60;
        unsigned long remaining_seconds = seconds_until_refresh % 60;
        snprintf(refresh_buffer, sizeof(refresh_buffer), "REFRESH %02lu:%02lu", remaining_minutes, remaining_seconds);
    } else {
        snprintf(refresh_buffer, sizeof(refresh_buffer), "REFRESH --:--");
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
        lv_color_hex(connected ? COLOR_OK : COLOR_ALERT),
        0
    );
}

void ui_update_page(const char* title, uint8_t page_index, uint8_t page_count) {
    apply_page_theme(page_index);

    if (page_index < TREND_PAGES) {
        active_trend_page = page_index;
        render_active_trends();
    }

    if (station_tag.label != nullptr) {
        char title_buffer[32];
        to_upper_copy(title, title_buffer, sizeof(title_buffer), 26);
        set_tag_text(station_tag, title_buffer);
    }

    uint8_t safeCount = page_count == 0 ? 1 : page_count;
    uint8_t safeIndex = page_index >= safeCount ? 0 : page_index;
    snprintf(
        traffic_page_text,
        sizeof(traffic_page_text),
        "%u/%u",
        static_cast<unsigned>(safeIndex + 1),
        static_cast<unsigned>(safeCount)
    );

    if (page_indicator_label != nullptr) {
        lv_label_set_text(page_indicator_label, traffic_page_text);
    }

    request_full_redraw();
}

void ui_show_portal_page(const char* ssid, const char* url) {
    ensure_portal_page_overlay();
    lv_obj_clear_flag(portal_page_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(portal_page_overlay);

    lv_label_set_text(portal_page_ssid_label, (ssid == nullptr || strlen(ssid) == 0) ? "-" : ssid);
    lv_label_set_text(portal_page_url_label, url == nullptr ? "http://-" : url);

    const char* qrPayload = (url == nullptr || strlen(url) == 0) ? "http://-" : url;
    lv_qrcode_update(portal_page_qr, qrPayload, strlen(qrPayload));

    request_full_redraw();
}

static void format_thousands(float value, char* out, size_t size) {
    const long rounded = lroundf(value);
    if (labs(rounded) < 1000) {
        snprintf(out, size, "%ld", rounded);
        return;
    }
    snprintf(out, size, "%ld,%03ld", rounded / 1000, labs(rounded % 1000));
}

void ui_show_traffic_page(const TrafficData& data, const AirlineLogo* logo, const RouteInfo* route) {
    ensure_traffic_overlay();
    lv_obj_clear_flag(traffic_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(traffic_overlay);

    lv_label_set_text(traffic_page_indicator, traffic_page_text);

    char buffer[48];
    snprintf(buffer, sizeof(buffer), "%u NM", static_cast<unsigned>(data.radiusNm));
    lv_label_set_text(traffic_range_label, buffer);

    if (!data.isValid || data.count == 0) {
        lv_label_set_text(traffic_callsign_label, data.isValid ? "NO CONTACTS" : "NO DATA");
        lv_label_set_text(traffic_airline_label, "");
        lv_label_set_text(traffic_aircraft_label, "");
        lv_label_set_text(traffic_route_label, "");
        lv_label_set_text(traffic_route_city_label, "");
        lv_obj_add_flag(traffic_logo_image, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(traffic_operator_tag.box, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < 4; ++i) {
            lv_label_set_text(traffic_metric_labels[i], "--");
        }
        for (int row = 0; row < TRAFFIC_LIST_ROWS; ++row) {
            lv_label_set_text(traffic_row_callsign[row], "");
            lv_label_set_text(traffic_row_airline[row], "");
            lv_label_set_text(traffic_row_alt[row], "");
            lv_label_set_text(traffic_row_speed[row], "");
            lv_label_set_text(traffic_row_track[row], "");
        }
        lv_label_set_text(
            traffic_footer_label,
            data.isValid ? "NOTHING IN RANGE" : "TRAFFIC UNAVAILABLE"
        );
        request_full_redraw();
        return;
    }

    const AircraftInfo& lead = data.aircraft[0];
    lv_label_set_text(traffic_callsign_label, lead.callsign);

    if (lead.airlineName != nullptr) {
        char name[40];
        to_upper_copy(lead.airlineName, name, sizeof(name), 0);
        lv_label_set_text(traffic_airline_label, name);
    } else if (lead.operatorIcao[0] != '\0') {
        lv_label_set_text(traffic_airline_label, lead.operatorIcao);
    } else {
        lv_label_set_text(traffic_airline_label, "GENERAL AVIATION");
    }

    if (lead.registration[0] != '\0' && lead.type[0] != '\0') {
        snprintf(buffer, sizeof(buffer), "%s - %s", lead.registration, lead.type);
    } else if (lead.type[0] != '\0') {
        snprintf(buffer, sizeof(buffer), "%s", lead.type);
    } else {
        snprintf(buffer, sizeof(buffer), "%s", lead.registration);
    }
    lv_label_set_text(traffic_aircraft_label, buffer);

    if (route != nullptr && route->isValid) {
        char cities[64];
        snprintf(buffer, sizeof(buffer), "%s  " LV_SYMBOL_RIGHT "  %s", route->fromCode, route->toCode);
        lv_label_set_text(traffic_route_label, buffer);
        to_upper_copy(route->fromCity, cities, sizeof(cities), 0);
        const size_t used = strlen(cities);
        snprintf(cities + used, sizeof(cities) - used, " - ");
        char destination[24];
        to_upper_copy(route->toCity, destination, sizeof(destination), 0);
        strncat(cities, destination, sizeof(cities) - strlen(cities) - 1);
        lv_label_set_text(traffic_route_city_label, cities);
    } else {
        lv_label_set_text(traffic_route_label, "ROUTE UNKNOWN");
        lv_label_set_text(traffic_route_city_label, "");
    }

    // Logo when we have one, otherwise fall back to the operator code.
    if (logo != nullptr && logo->isValid && logo->png != nullptr) {
        // RAW hands the encoded PNG to LVGL's decoder. The cache is keyed on the
        // descriptor, so drop the old entry or a previous airline's logo is
        // redrawn from stale pixels. Only when it actually changed, though:
        // dropping it forces a PNG decode on every render.
        static const uint8_t* shown_logo = nullptr;
        const bool logo_changed = shown_logo != logo->png;
        if (logo_changed) {
            lv_image_cache_drop(&traffic_logo_dsc);
            shown_logo = logo->png;
        }

        traffic_logo_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        traffic_logo_dsc.header.cf = LV_COLOR_FORMAT_RAW;
        traffic_logo_dsc.header.flags = 0;
        traffic_logo_dsc.header.w = logo->width;
        traffic_logo_dsc.header.h = logo->height;
        traffic_logo_dsc.header.stride = 0;
        traffic_logo_dsc.data = logo->png;
        traffic_logo_dsc.data_size = logo->pngSize;

        if (logo_changed) {
            lv_image_set_src(traffic_logo_image, &traffic_logo_dsc);
        }
        lv_obj_clear_flag(traffic_logo_image, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(traffic_operator_tag.box, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(traffic_logo_image, LV_OBJ_FLAG_HIDDEN);
        if (lead.operatorIcao[0] != '\0') {
            set_tag_text(traffic_operator_tag, lead.operatorIcao);
            lv_obj_clear_flag(traffic_operator_tag.box, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(traffic_operator_tag.box, LV_OBJ_FLAG_HIDDEN);
        }
    }


    if (lead.onGround) {
        lv_label_set_text(traffic_metric_labels[0], "GND");
    } else {
        format_thousands(lead.altitudeFt, buffer, sizeof(buffer));
        lv_label_set_text(traffic_metric_labels[0], buffer);
    }

    snprintf(buffer, sizeof(buffer), "%.0f", lead.groundSpeedKt);
    lv_label_set_text(traffic_metric_labels[1], buffer);

    snprintf(buffer, sizeof(buffer), "%.1f", lead.distanceNm);
    lv_label_set_text(traffic_metric_labels[2], buffer);

    snprintf(buffer, sizeof(buffer), "%.0f\xC2\xB0", lead.trackDeg);
    lv_label_set_text(traffic_metric_labels[3], buffer);

    for (int row = 0; row < TRAFFIC_LIST_ROWS; ++row) {
        const uint8_t index = static_cast<uint8_t>(row + 1);
        if (index >= data.count) {
            lv_label_set_text(traffic_row_callsign[row], "");
            lv_label_set_text(traffic_row_airline[row], "");
            lv_label_set_text(traffic_row_alt[row], "");
            lv_label_set_text(traffic_row_speed[row], "");
            lv_label_set_text(traffic_row_track[row], "");
            continue;
        }

        const AircraftInfo& other = data.aircraft[index];
        lv_label_set_text(traffic_row_callsign[row], other.callsign);

        if (other.hasRoute) {
            snprintf(buffer, sizeof(buffer), "%s " LV_SYMBOL_RIGHT " %s", other.routeFrom, other.routeTo);
            lv_label_set_text(traffic_row_airline[row], buffer);
        } else {
            lv_label_set_text(traffic_row_airline[row], "-");
        }

        char altitude[16];
        if (other.onGround) {
            snprintf(altitude, sizeof(altitude), "GND");
        } else {
            format_thousands(other.altitudeFt, altitude, sizeof(altitude));
        }
        snprintf(buffer, sizeof(buffer), "%s ft", altitude);
        lv_label_set_text(traffic_row_alt[row], buffer);

        snprintf(buffer, sizeof(buffer), "%.0f kt", other.groundSpeedKt);
        lv_label_set_text(traffic_row_speed[row], buffer);

        snprintf(buffer, sizeof(buffer), "%.0f\xC2\xB0", other.trackDeg);
        lv_label_set_text(traffic_row_track[row], buffer);
    }

    // The feed times out now and then; rather than blanking the page, say how
    // stale the list is once it is more than a refresh interval old.
    if (data.ageSeconds > 60) {
        snprintf(
            buffer,
            sizeof(buffer),
            "%u AIRBORNE WITHIN %u NM - %um AGO",
            static_cast<unsigned>(data.totalInRange),
            static_cast<unsigned>(data.radiusNm),
            static_cast<unsigned>(data.ageSeconds / 60)
        );
    } else {
        snprintf(
            buffer,
            sizeof(buffer),
            "%u AIRBORNE WITHIN %u NM",
            static_cast<unsigned>(data.totalInRange),
            static_cast<unsigned>(data.radiusNm)
        );
    }
    lv_label_set_text(traffic_footer_label, buffer);

    request_full_redraw();
}

void ui_hide_traffic_page() {
    if (traffic_overlay == nullptr) {
        return;
    }

    lv_obj_add_flag(traffic_overlay, LV_OBJ_FLAG_HIDDEN);
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
    set_status("WAIT", COLOR_WARN);
    ui_update_clock("--:--:--", "-- --- ----", 0, false);
    request_full_redraw();
}

void ui_show_error(const char* message) {
    LV_UNUSED(message);
    set_status("ERROR", COLOR_ALERT);
    ui_update_clock("--:--:--", "-- --- ----", 0, false);
    request_full_redraw();
}

void ui_show_setup(const char* ap_ssid, const char* ap_password, const char* url) {
    LV_UNUSED(ap_password);

    ensure_setup_overlay();
    lv_obj_clear_flag(setup_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(setup_overlay);

    String wifiLabel = ap_ssid;
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
