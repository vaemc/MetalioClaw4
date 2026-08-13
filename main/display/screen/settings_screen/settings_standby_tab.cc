#include "settings_standby_tab.h"

#include "home_screen/home_screen.h"
#include "i18n.h"
#include "settings_common.h"

LV_FONT_DECLARE(font_puhui_20_4);

namespace {

struct StandbyUi {
    lv_obj_t* enter_standby_min_label = nullptr;
    lv_obj_t* enter_standby_slider = nullptr;
    lv_obj_t* shutdown_min_label = nullptr;
    lv_obj_t* shutdown_slider = nullptr;
};
StandbyUi s_ui;

void OnEnterStandbySliderChanged(lv_event_t* e) {
    auto* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int value = static_cast<int>(lv_slider_get_value(slider));
    UpdateMinutesLabel(s_ui.enter_standby_min_label, value, I18n::T("永不进入"));
    HomeScreen::SetIdleStandbyMinutes(value);
}

void OnShutdownSliderChanged(lv_event_t* e) {
    auto* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int value = static_cast<int>(lv_slider_get_value(slider));
    UpdateMinutesLabel(s_ui.shutdown_min_label, value, I18n::T("永不关机"));
    HomeScreen::SetIdleShutdownMinutes(value);
}

}  // namespace

void SettingsStandbyTab_Build(lv_obj_t* tab) {
    const int initial_standby = HomeScreen::GetIdleStandbyMinutes();
    const int initial_shutdown = HomeScreen::GetIdleShutdownMinutes();
    constexpr int kStandbyCardH = 132;

    BuildSliderPanel(tab, I18n::T("进入待机"), I18n::T("首页无操作后进入待机页"),
                     I18n::T("0 ~ 60 分钟"), initial_standby, &s_ui.enter_standby_min_label,
                     &s_ui.enter_standby_slider, 0, 60, OnEnterStandbySliderChanged, kStandbyCardH);
    UpdateMinutesLabel(s_ui.enter_standby_min_label, initial_standby, I18n::T("永不进入"));

    BuildSliderPanel(tab, I18n::T("自动关机"), I18n::T("首页+待机累计无操作后关机"),
                     I18n::T("0 ~ 60 分钟"), initial_shutdown, &s_ui.shutdown_min_label,
                     &s_ui.shutdown_slider, 0, 60, OnShutdownSliderChanged, kStandbyCardH);
    UpdateMinutesLabel(s_ui.shutdown_min_label, initial_shutdown, I18n::T("永不关机"));

    // 待机 Tab 内容更密：略收紧行距，并加大底部留白。
    lv_obj_set_style_pad_row(tab, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(tab, 56, LV_PART_MAIN);

    lv_obj_t* foot = lv_label_create(tab);
    lv_label_set_text(foot, I18n::T("待机设置会自动保存"));
    lv_obj_set_style_text_color(foot, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(foot, &font_puhui_20_4, LV_PART_MAIN);
}
