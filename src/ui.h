#ifndef UI_H
#define UI_H

#include <lvgl.h>
#include "weather.h"

void ui_init();
void ui_update_weather(const WeatherData& data);
void ui_update_astronomy(const char* sunrise, const char* sunset, float moon_phase);
void ui_update_clock(const char* time_text, const char* date_text, unsigned long seconds_until_refresh, bool has_refresh_schedule);
void ui_update_wifi_status(bool connected);
void ui_update_page(const char* title, uint8_t page_index, uint8_t page_count);
void ui_show_portal_page(const char* ssid, const char* url);
void ui_hide_portal_page();
void ui_show_loading();
void ui_show_error(const char* message);
void ui_show_setup(const char* ap_ssid, const char* ap_password, const char* url);
void ui_hide_setup();

#endif
