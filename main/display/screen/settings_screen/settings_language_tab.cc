#include "settings_language_tab.h"

#include <cstdint>

#include <esp_log.h>

#include "home_screen/home_screen.h"
#include "i18n.h"
#include "screen_util.h"
#include "settings_common.h"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_puhui_30_4);

namespace {

constexpr const char* TAG = "SettingsLanguage";

void GoHomeAfterLocaleChange() {
    HomeScreen::ResetToFirstPage();
    lv_obj_t* old_scr = lv_screen_active();
    lv_obj_t* home = HomeScreen::Create();
    lv_screen_load(home);
    if (old_scr != nullptr && old_scr != home) {
        lv_obj_delete_async(old_scr);
    }
}

void OnLanguageCardClicked(lv_event_t* e) {
    auto locale =
        static_cast<I18n::Locale>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    if (!I18n::SetLocale(locale)) {
        return;
    }
    ESP_LOGI(TAG, "language -> %s (rebuild home)", I18n::GetLocaleCode());
    GoHomeAfterLocaleChange();
}

}  // namespace

void SettingsLanguageTab_Build(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 24, LV_PART_MAIN);
    lv_obj_set_style_pad_row(tab, 16, LV_PART_MAIN);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hint = lv_label_create(tab);
    lv_label_set_text(hint, I18n::T("选择界面显示语言"));
    lv_obj_set_style_text_color(hint, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &font_puhui_20_4, LV_PART_MAIN);

    const I18n::Locale current = I18n::GetLocale();
    for (size_t i = 0; i < I18n::GetLocaleCount(); ++i) {
        const I18n::LocaleInfo* info = I18n::GetLocaleInfo(static_cast<I18n::Locale>(i));
        if (info == nullptr) {
            continue;
        }

        lv_obj_t* card = lv_obj_create(tab);
        screen_strip_obj_chrome(card);
        lv_obj_set_width(card, LV_PCT(100));
        lv_obj_set_height(card, 88);
        lv_obj_set_style_bg_color(card, lv_color_hex(kColorCard), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 20, LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
        const bool selected = (info->id == current);
        lv_obj_set_style_border_color(card, lv_color_hex(selected ? kColorAccent : kColorCard),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_opa(card, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, OnLanguageCardClicked, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(
                                static_cast<uintptr_t>(static_cast<unsigned>(info->id))));
        screen_swipe_back_ignore(card, true);

        lv_obj_t* name = lv_label_create(card);
        // Native name stays in its own script (简体中文 / English).
        lv_label_set_text(name, info->native_name);
        lv_obj_set_style_text_color(name, lv_color_hex(kColorText), LV_PART_MAIN);
        lv_obj_set_style_text_font(name, &font_puhui_30_4, LV_PART_MAIN);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 20, -10);

        lv_obj_t* code = lv_label_create(card);
        lv_label_set_text(code, info->english_name);
        lv_obj_set_style_text_color(code, lv_color_hex(kColorSubtle), LV_PART_MAIN);
        lv_obj_set_style_text_font(code, &font_puhui_20_4, LV_PART_MAIN);
        lv_obj_align(code, LV_ALIGN_LEFT_MID, 20, 18);

        if (selected) {
            lv_obj_t* mark = lv_label_create(card);
            lv_label_set_text(mark, I18n::T("当前语言"));
            lv_obj_set_style_text_color(mark, lv_color_hex(kColorValue), LV_PART_MAIN);
            lv_obj_set_style_text_font(mark, &font_puhui_20_4, LV_PART_MAIN);
            lv_obj_align(mark, LV_ALIGN_RIGHT_MID, -20, 0);
        }
    }

    lv_obj_t* foot = lv_label_create(tab);
    lv_label_set_text(foot, I18n::T("切换后立即生效并返回主页"));
    lv_obj_set_style_text_color(foot, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(foot, &font_puhui_20_4, LV_PART_MAIN);
}
