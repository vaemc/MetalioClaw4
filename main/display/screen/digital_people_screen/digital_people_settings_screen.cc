#include "digital_people_settings_screen.h"
#include "digital_people_prefs.h"
#include "digital_people_screen.h"
#include "i18n.h"

#include <cstdio>

#include "esp_log.h"

#include "pwr_key_handler.h"
#include "screen_util.h"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_puhui_30_4);

namespace {

constexpr const char* TAG = "DigitalPeopleSettings";

constexpr int32_t kPanelSize = 720;
constexpr int32_t kHeaderH = 90;
constexpr int32_t kBackBtnSize = 72;
constexpr int32_t kContentTop = 100;
constexpr int32_t kPad = 16;
constexpr int32_t kFormatCardH = 88;
constexpr int32_t kDelayValueCardH = 100;

constexpr uint32_t kColorBg = 0x0E1116;
constexpr uint32_t kColorCard = 0x1B2030;
constexpr uint32_t kColorAccent = 0x3B82F6;
constexpr uint32_t kColorText = 0xFFFFFF;
constexpr uint32_t kColorSubtle = 0x9AA3B2;
constexpr uint32_t kColorValue = 0x60A5FA;
constexpr uint32_t kColorSliderTrack = 0x2A2F3A;

struct UiState {
    lv_obj_t* screen = nullptr;
    lv_obj_t* content = nullptr;
    lv_obj_t* sjpg_card = nullptr;
    lv_obj_t* eaf_card = nullptr;
    lv_obj_t* sjpg_mark = nullptr;
    lv_obj_t* eaf_mark = nullptr;
    lv_obj_t* delay_section = nullptr;
    lv_obj_t* delay_value_label = nullptr;
    lv_obj_t* delay_slider = nullptr;
    DigitalPeoplePrefs::EmotionFormat format =
        DigitalPeoplePrefs::EmotionFormat::Sjpg;
};
UiState s_ui;

void digital_people_lifecycle_cb(screen_lifecycle_event_t event) {
    PwrKey_OnScreenLifecycle("digital_people", event);
    DigitalPeopleScreen::LifecycleCallback(event);
}

void UpdateFormatSelectionUi();
void UpdateDelaySectionVisibility();
void UpdateDelayValueLabel(uint32_t delay_ms);

void StyleSlider(lv_obj_t* slider) {
    lv_obj_set_width(slider, LV_PCT(100));
    lv_obj_set_height(slider, 24);
    lv_obj_set_style_bg_color(slider, lv_color_hex(kColorSliderTrack),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(kColorAccent),
                              LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_pad_hor(slider, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(slider, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(slider, 3, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, 10, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 10, LV_PART_INDICATOR);
}

void UpdateDelayValueLabel(uint32_t delay_ms) {
    if (s_ui.delay_value_label == nullptr) {
        return;
    }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(delay_ms));
    lv_label_set_text(s_ui.delay_value_label, buf);
}

void OnDelaySliderChanged(lv_event_t* e) {
    auto* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    const uint32_t value =
        static_cast<uint32_t>(lv_slider_get_value(slider));
    DigitalPeoplePrefs::SetFrameDelayMs(value);
    UpdateDelayValueLabel(DigitalPeoplePrefs::GetFrameDelayMs());
    ESP_LOGI(TAG, "frame_delay -> %u ms",
             static_cast<unsigned>(DigitalPeoplePrefs::GetFrameDelayMs()));
}

void ApplyFormat(DigitalPeoplePrefs::EmotionFormat format) {
    if (format == s_ui.format) {
        return;
    }
    s_ui.format = format;
    DigitalPeoplePrefs::SetEmotionFormat(format);
    ESP_LOGI(TAG, "emotion format -> %s",
             DigitalPeoplePrefs::GetEmotionExt());
    UpdateFormatSelectionUi();
    UpdateDelaySectionVisibility();
}

void OnFormatCardClicked(lv_event_t* e) {
    const auto format = static_cast<DigitalPeoplePrefs::EmotionFormat>(
        reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    ApplyFormat(format);
}

void StyleFormatCard(lv_obj_t* card, lv_obj_t* mark, bool selected) {
    if (card != nullptr) {
        lv_obj_set_style_border_color(
            card, lv_color_hex(selected ? kColorAccent : kColorCard),
            LV_PART_MAIN);
    }
    if (mark != nullptr) {
        if (selected) {
            lv_obj_remove_flag(mark, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(mark, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void UpdateFormatSelectionUi() {
    StyleFormatCard(s_ui.sjpg_card, s_ui.sjpg_mark,
                    s_ui.format == DigitalPeoplePrefs::EmotionFormat::Sjpg);
    StyleFormatCard(s_ui.eaf_card, s_ui.eaf_mark,
                    s_ui.format == DigitalPeoplePrefs::EmotionFormat::Eaf);
}

void UpdateDelaySectionVisibility() {
    if (s_ui.delay_section == nullptr) {
        return;
    }
    if (s_ui.format == DigitalPeoplePrefs::EmotionFormat::Eaf) {
        lv_obj_remove_flag(s_ui.delay_section, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_ui.delay_section, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t* BuildFormatCard(lv_obj_t* parent, const char* title,
                          const char* subtitle,
                          DigitalPeoplePrefs::EmotionFormat format,
                          lv_obj_t** mark_out) {
    const bool selected = (format == s_ui.format);

    lv_obj_t* card = lv_obj_create(parent);
    screen_strip_obj_chrome(card);
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_height(card, kFormatCardH);
    lv_obj_set_style_bg_color(card, lv_color_hex(kColorCard), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 16, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        card, lv_color_hex(selected ? kColorAccent : kColorCard), LV_PART_MAIN);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, OnFormatCardClicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(
                            static_cast<unsigned>(format))));

    lv_obj_t* name = lv_label_create(card);
    lv_label_set_text(name, title);
    lv_obj_set_style_text_color(name, lv_color_hex(kColorText), LV_PART_MAIN);
    lv_obj_set_style_text_font(name, &font_puhui_20_4, LV_PART_MAIN);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 14, 14);

    lv_obj_t* sub = lv_label_create(card);
    lv_label_set_text(sub, subtitle);
    lv_label_set_long_mode(sub, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(sub, LV_PCT(90));
    lv_obj_set_style_text_color(sub, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(sub, &font_puhui_20_4, LV_PART_MAIN);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 14, -14);

    lv_obj_t* mark = lv_label_create(card);
    if (mark_out != nullptr) {
        *mark_out = mark;
    }
    lv_label_set_text(mark, I18n::T("当前格式"));
    lv_obj_set_style_text_color(mark, lv_color_hex(kColorAccent), LV_PART_MAIN);
    lv_obj_set_style_text_font(mark, &font_puhui_20_4, LV_PART_MAIN);
    lv_obj_align(mark, LV_ALIGN_TOP_RIGHT, -10, 12);
    if (!selected) {
        lv_obj_add_flag(mark, LV_OBJ_FLAG_HIDDEN);
    }

    return card;
}

void GoBackToDigitalPeople() {
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr) {
        lv_indev_wait_release(indev);
    }
    lv_obj_t* old_scr = lv_screen_active();
    lv_obj_t* dp = DigitalPeopleScreen::Create();
    screen_attach_lifecycle(dp, digital_people_lifecycle_cb);
    lv_screen_load(dp);
    if (old_scr != nullptr && old_scr != dp) {
        lv_obj_delete_async(old_scr);
    }
}

void OnBackClicked(lv_event_t* /*e*/) { GoBackToDigitalPeople(); }

void OnScreenUnloaded(lv_event_t* e) {
    if (lv_event_get_target(e) != s_ui.screen) {
        return;
    }
    s_ui = UiState{};
}

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
    lv_obj_set_style_bg_color(back, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(back, LV_OPA_20, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(back, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(back, OnBackClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* back_icon = lv_image_create(back);
    lv_image_set_src(back_icon, "A:ic_app_back.spng");
    lv_obj_remove_flag(back_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(back_icon);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, I18n::T("数字人设置"));
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &font_puhui_30_4, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 16 + kBackBtnSize + 16, 0);
}

void BuildDelaySection(lv_obj_t* parent, uint32_t initial_delay) {
    lv_obj_t* section = lv_obj_create(parent);
    screen_strip_obj_chrome(section);
    s_ui.delay_section = section;
    lv_obj_set_width(section, LV_PCT(100));
    lv_obj_set_height(section, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(section, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(section, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(section, 8, LV_PART_MAIN);
    lv_obj_remove_flag(section, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* card = lv_obj_create(section);
    screen_strip_obj_chrome(card);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, kDelayValueCardH);
    lv_obj_set_style_bg_color(card, lv_color_hex(kColorCard), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 16, LV_PART_MAIN);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    screen_make_input_passive(card);

    lv_obj_t* value = lv_label_create(card);
    s_ui.delay_value_label = value;
    lv_obj_set_width(value, LV_PCT(100));
    lv_label_set_long_mode(value, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(value, lv_color_hex(kColorValue), LV_PART_MAIN);
    lv_obj_set_style_text_font(value, &font_puhui_30_4, LV_PART_MAIN);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(value, LV_ALIGN_CENTER, 0, -8);
    UpdateDelayValueLabel(initial_delay);

    lv_obj_t* hint = lv_label_create(card);
    lv_label_set_text(hint, I18n::T("当前帧间隔 (ms)"));
    lv_obj_set_style_text_color(hint, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &font_puhui_20_4, LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t* slider_hdr = lv_obj_create(section);
    lv_obj_remove_style_all(slider_hdr);
    lv_obj_set_width(slider_hdr, LV_PCT(100));
    lv_obj_set_height(slider_hdr, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(slider_hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(slider_hdr, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(slider_hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* slider_title = lv_label_create(slider_hdr);
    lv_label_set_text(slider_title, I18n::T("帧间隔"));
    lv_obj_set_style_text_color(slider_title, lv_color_hex(kColorText),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(slider_title, &font_puhui_20_4, LV_PART_MAIN);

    lv_obj_t* range_lbl = lv_label_create(slider_hdr);
    lv_label_set_text(range_lbl, I18n::T("10 ~ 500 毫秒"));
    lv_obj_set_style_text_color(range_lbl, lv_color_hex(kColorSubtle),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(range_lbl, &font_puhui_20_4, LV_PART_MAIN);

    lv_obj_t* row = lv_obj_create(section);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 44);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* slider = lv_slider_create(row);
    s_ui.delay_slider = slider;
    StyleSlider(slider);
    lv_slider_set_range(slider,
                        static_cast<int32_t>(DigitalPeoplePrefs::kMinFrameDelayMs),
                        static_cast<int32_t>(DigitalPeoplePrefs::kMaxFrameDelayMs));
    lv_slider_set_value(slider, static_cast<int32_t>(initial_delay), LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(slider, OnDelaySliderChanged, LV_EVENT_VALUE_CHANGED,
                        nullptr);

    UpdateDelaySectionVisibility();
}

}  // namespace

lv_obj_t* DigitalPeopleSettingsScreen::Create() {
    s_ui = UiState{};
    s_ui.format = DigitalPeoplePrefs::GetEmotionFormat();
    const uint32_t delay_ms = DigitalPeoplePrefs::GetFrameDelayMs();

    ESP_LOGI(TAG, "create settings (fmt=%s delay=%u)",
             DigitalPeoplePrefs::GetEmotionExt(),
             static_cast<unsigned>(delay_ms));

    lv_obj_t* scr = lv_obj_create(nullptr);
    s_ui.screen = scr;
    screen_strip_obj_chrome(scr);
    lv_obj_set_size(scr, kPanelSize, kPanelSize);
    lv_obj_set_style_bg_color(scr, lv_color_hex(kColorBg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    BuildHeader(scr);

    lv_obj_t* content = lv_obj_create(scr);
    screen_strip_obj_chrome(content);
    s_ui.content = content;
    lv_obj_set_size(content, kPanelSize, kPanelSize - kContentTop);
    lv_obj_set_pos(content, 0, kContentTop);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(content, kPad, LV_PART_MAIN);
    lv_obj_set_style_pad_top(content, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(content, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_row(content, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* section_title = lv_label_create(content);
    lv_label_set_text(section_title, I18n::T("表情资源格式"));
    lv_obj_set_style_text_color(section_title, lv_color_hex(kColorText),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(section_title, &font_puhui_20_4, LV_PART_MAIN);

    lv_obj_t* section_hint = lv_label_create(content);
    lv_label_set_text(section_hint,
                      I18n::T("选择 EAF 动画或 SJPG 静态图"));
    lv_obj_set_style_text_color(section_hint, lv_color_hex(kColorSubtle),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(section_hint, &font_puhui_20_4, LV_PART_MAIN);

    lv_obj_t* format_row = lv_obj_create(content);
    lv_obj_remove_style_all(format_row);
    lv_obj_set_width(format_row, LV_PCT(100));
    lv_obj_set_height(format_row, kFormatCardH);
    lv_obj_set_flex_flow(format_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(format_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(format_row, 12, LV_PART_MAIN);
    lv_obj_remove_flag(format_row, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.eaf_card =
        BuildFormatCard(format_row, I18n::T("EAF 动画"),
                        I18n::T("加载 SD 卡 .eaf 素材"),
                        DigitalPeoplePrefs::EmotionFormat::Eaf, &s_ui.eaf_mark);
    s_ui.sjpg_card =
        BuildFormatCard(format_row, I18n::T("SJPG 静态图"),
                        I18n::T("加载 SD 卡 .sjpg 素材"),
                        DigitalPeoplePrefs::EmotionFormat::Sjpg,
                        &s_ui.sjpg_mark);

    BuildDelaySection(content, delay_ms);

    lv_obj_t* foot = lv_label_create(content);
    lv_label_set_text(foot, I18n::T("数字人设置会自动保存"));
    lv_obj_set_style_text_color(foot, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(foot, &font_puhui_20_4, LV_PART_MAIN);

    screen_attach_lifecycle(scr, [](screen_lifecycle_event_t event) {
        PwrKey_OnScreenLifecycle("digital_people_settings", event);
    });
    lv_obj_add_event_cb(scr, OnScreenUnloaded, LV_EVENT_SCREEN_UNLOADED,
                        nullptr);

    return scr;
}
