#include "settings_screen.h"

#include <esp_log.h>

#include "bluetooth_screen/bluetooth_screen.h"
#include "cx25601n.h"
#include "home_screen/home_screen.h"
#include "i18n.h"
#include "screen_util.h"
#include "settings_bluetooth_tab.h"
#include "settings_brightness_tab.h"
#include "settings_upgrade_tab.h"
#include "settings_charge_tab.h"
#include "settings_common.h"
#include "settings_language_tab.h"
#include "settings_standby_tab.h"
#include "settings_volume_tab.h"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_puhui_30_4);

namespace {

constexpr const char* TAG = "SettingsScreen";

struct ShellUi {
    lv_obj_t* screen = nullptr;
    lv_obj_t* tabview = nullptr;
};
ShellUi s_ui;

void OnSwipeBack();
void OnBackClicked(lv_event_t* e);

void BuildHeader(lv_obj_t* parent) {
    lv_obj_t* header = lv_obj_create(parent);
    screen_strip_obj_chrome(header);
    lv_obj_set_size(header, kPanelSize, kHeaderH);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* back = lv_button_create(header);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, kBackBtnSize, kBackBtnSize);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(back, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(back, LV_OPA_20, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(back, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(back, OnBackClicked, LV_EVENT_CLICKED, nullptr);
    screen_swipe_back_ignore(back, true);

    lv_obj_t* back_icon = lv_image_create(back);
    lv_image_set_src(back_icon, "A:ic_app_back.spng");
    lv_obj_remove_flag(back_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(back_icon);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, I18n::T("设置"));
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &font_puhui_30_4, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 16 + kBackBtnSize + 16, 0);
}

void FixTabBarItemHeights(lv_obj_t* tabview) {
    lv_obj_t* bar = lv_tabview_get_tab_bar(tabview);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(bar, kTabItemGap, LV_PART_MAIN);
    lv_obj_set_style_pad_top(bar, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(bar, 8, LV_PART_MAIN);

    const uint32_t count = lv_tabview_get_tab_count(tabview);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t* btn = lv_obj_get_child_by_type(bar, i, &lv_button_class);
        if (btn == nullptr) {
            continue;
        }
        lv_obj_set_flex_grow(btn, 0);
        lv_obj_set_width(btn, lv_pct(100));
        lv_obj_set_height(btn, kTabItemH);
        lv_obj_set_style_radius(btn, 12, LV_PART_MAIN);
    }
}

void BuildTabView(lv_obj_t* parent) {
    const int initial_brightness = ReadInitialBrightness();
    const int initial_volume = ReadInitialVolume();

    lv_obj_t* tv = lv_tabview_create(parent);
    s_ui.tabview = tv;
    lv_obj_set_size(tv, kPanelSize, kBodyH);
    lv_obj_set_pos(tv, 0, kHeaderH);
    lv_tabview_set_tab_bar_position(tv, LV_DIR_LEFT);
    lv_tabview_set_tab_bar_size(tv, kTabBarW);

    lv_obj_set_style_bg_color(tv, lv_color_hex(kColorBg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tv, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(tv, 0, LV_PART_MAIN);

    lv_obj_t* bar = lv_tabview_get_tab_bar(tv);
    lv_obj_set_style_bg_color(bar, lv_color_hex(kColorTabBar), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(bar, lv_color_hex(kColorText), LV_PART_MAIN);
    lv_obj_set_style_text_font(bar, &font_puhui_20_4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_hor(bar, 6, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(bar, lv_color_hex(kColorAccent), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(bar, lv_color_hex(kColorText), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_t* content = lv_tabview_get_content(tv);
    screen_swipe_back_ignore(content, true);

    lv_obj_t* tab_brightness = lv_tabview_add_tab(tv, I18n::T("亮度"));
    SettingsBrightnessTab_Build(tab_brightness, initial_brightness);

    lv_obj_t* tab_standby = lv_tabview_add_tab(tv, I18n::T("待机"));
    SettingsStandbyTab_Build(tab_standby);

    lv_obj_t* tab_volume = lv_tabview_add_tab(tv, I18n::T("音量"));
    SettingsVolumeTab_Build(tab_volume, initial_volume);

    lv_obj_t* tab_language = lv_tabview_add_tab(tv, I18n::T("语言"));
    SettingsLanguageTab_Build(tab_language);

    // 老设备无 CX25601N（0x6B）时不显示充电 Tab
    if (cx25601n_is_ready()) {
        lv_obj_t* tab_charge = lv_tabview_add_tab(tv, I18n::T("充电"));
        SettingsChargeTab_Build(tab_charge);
    }

#if CONFIG_ESP_HOSTED_ENABLED
    lv_obj_t* tab_upgrade = lv_tabview_add_tab(tv, I18n::T("升级"));
    SettingsUpgradeTab_Build(tab_upgrade);
#endif

    lv_obj_t* tab_bluetooth = lv_tabview_add_tab(tv, I18n::T("蓝牙"));
    SettingsBluetoothTab_Build(tab_bluetooth);

    FixTabBarItemHeights(tv);
}

void OnSwipeBack() {
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr) {
        lv_indev_wait_release(indev);
    }
    lv_obj_t* old_scr = lv_screen_active();
    lv_obj_t* home = HomeScreen::Create();
    lv_screen_load(home);
    if (old_scr != nullptr && old_scr != home) {
        lv_obj_delete_async(old_scr);
    }
}

void OnBackClicked(lv_event_t* /*e*/) { OnSwipeBack(); }

void OnScreenUnloaded(lv_event_t* /*e*/) {
    BluetoothScreen::ResetUi();
    SettingsChargeTab_Reset();
#if CONFIG_ESP_HOSTED_ENABLED
    SettingsUpgradeTab_Reset();
#endif
    s_ui.screen = nullptr;
    s_ui.tabview = nullptr;
}

}  // namespace

lv_obj_t* SettingsScreen::Create() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    s_ui.screen = scr;
    screen_strip_obj_chrome(scr);
    lv_obj_set_size(scr, kPanelSize, kPanelSize);
    lv_obj_set_style_bg_color(scr, lv_color_hex(kColorBg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    BuildHeader(scr);
    BuildTabView(scr);

    screen_attach_swipe_back(scr, OnSwipeBack);
    lv_obj_add_event_cb(scr, OnScreenUnloaded, LV_EVENT_SCREEN_UNLOADED, nullptr);

    return scr;
}

void SettingsScreen::LifecycleCallback(screen_lifecycle_event_t event) {
    if (event == SCREEN_LIFECYCLE_LOAD) {
        ESP_LOGI(TAG, "load: settings_screen");
    } else {
        ESP_LOGI(TAG, "unload: settings_screen");
    }
    BluetoothScreen::LifecycleCallback(event);
}
