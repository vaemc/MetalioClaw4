#include "settings_volume_tab.h"

#include "i18n.h"
#include "settings_common.h"

LV_FONT_DECLARE(font_puhui_20_4);

namespace {

struct VolumeUi {
    lv_obj_t* pct_label = nullptr;
    lv_obj_t* slider = nullptr;
};
VolumeUi s_ui;

void OnVolumeSliderChanged(lv_event_t* e) {
    auto* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int value = static_cast<int>(lv_slider_get_value(slider));
    UpdatePctLabel(s_ui.pct_label, value);
    ApplyVolume(value);
    if (value != static_cast<int>(lv_slider_get_value(slider))) {
        lv_slider_set_value(slider, value, LV_ANIM_OFF);
    }
}

}  // namespace

void SettingsVolumeTab_Build(lv_obj_t* tab, int initial_volume) {
    BuildSliderPanel(tab, I18n::T("拖动调节"), I18n::T("当前音量"), "0% ~ 100%", initial_volume,
                     &s_ui.pct_label, &s_ui.slider, 0, 100, OnVolumeSliderChanged);

    lv_obj_t* foot = lv_label_create(tab);
    lv_label_set_text(foot, I18n::T("音量设置会自动保存"));
    lv_obj_set_style_text_color(foot, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(foot, &font_puhui_20_4, LV_PART_MAIN);
}
