#pragma once

#include "lvgl.h"

#include <cstdint>

// Layout (720x720 panel)
constexpr int kPanelSize = 720;
constexpr int kHeaderH = 90;
constexpr int kBackBtnSize = 72;
constexpr int kTabBarW = 120;
constexpr int kTabItemH = 64;
constexpr int kTabItemGap = 10;
constexpr int kBodyH = kPanelSize - kHeaderH;

// Dark theme colors
constexpr uint32_t kColorBg = 0x0E1116;
constexpr uint32_t kColorText = 0xFFFFFF;
constexpr uint32_t kColorSubtle = 0x9AA3B2;
constexpr uint32_t kColorCard = 0x1B2030;
constexpr uint32_t kColorTabBar = 0x12151C;
constexpr uint32_t kColorAccent = 0x3B82F6;
constexpr uint32_t kColorValue = 0x60A5FA;
constexpr uint32_t kColorSliderTrack = 0x2A2F3A;

int ReadInitialBrightness();
int ReadInitialVolume();

void UpdatePctLabel(lv_obj_t* label, int pct);
void UpdateMinutesLabel(lv_obj_t* label, int minutes, const char* never_text);

void ApplyBrightness(int value);
void ApplyVolume(int volume);

void StyleSlider(lv_obj_t* slider);
lv_obj_t* CreateSliderRow(lv_obj_t* parent, int min_value, int max_value, int initial_value,
                          lv_event_cb_t cb, lv_obj_t** out_slider);
void BuildSliderPanel(lv_obj_t* parent, const char* title, const char* hint, const char* range_hint,
                      int initial_value, lv_obj_t** pct_label_out, lv_obj_t** slider_out,
                      int slider_min, int slider_max, lv_event_cb_t slider_cb,
                      int card_height = 180);
