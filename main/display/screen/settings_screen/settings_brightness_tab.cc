#include "settings_brightness_tab.h"

#include <cstdio>

#include "backlight.h"
#include "i18n.h"
#include "settings_common.h"

LV_FONT_DECLARE(font_puhui_20_4);

namespace {

struct BrightnessUi {
    lv_obj_t* pct_label = nullptr;
    lv_obj_t* slider = nullptr;
};
BrightnessUi s_ui;

void OnBrightnessSliderChanged(lv_event_t* e) {
    auto* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int value = static_cast<int>(lv_slider_get_value(slider));
    if (value < static_cast<int>(kBacklightMinPercent)) {
        value = kBacklightMinPercent;
        lv_slider_set_value(slider, value, LV_ANIM_OFF);
    }
    UpdatePctLabel(s_ui.pct_label, value);
    ApplyBrightness(value);
}

}  // namespace

void SettingsBrightnessTab_Build(lv_obj_t* tab, int initial_brightness) {
    char range_buf[24];
    std::snprintf(range_buf, sizeof(range_buf), "%d%% ~ 100%%",
                  static_cast<int>(kBacklightMinPercent));
    BuildSliderPanel(tab, I18n::T("拖动调节"), I18n::T("当前亮度"), range_buf, initial_brightness,
                     &s_ui.pct_label, &s_ui.slider, static_cast<int>(kBacklightMinPercent), 100,
                     OnBrightnessSliderChanged);

    lv_obj_t* foot = lv_label_create(tab);
    lv_label_set_text(foot, I18n::T("亮度设置会自动保存"));
    lv_obj_set_style_text_color(foot, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(foot, &font_puhui_20_4, LV_PART_MAIN);
}
