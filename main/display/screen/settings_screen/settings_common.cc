#include "settings_common.h"

#include <cstdio>

#include "audio_codec.h"
#include "backlight.h"
#include "board.h"
#include "i18n.h"
#include "screen_util.h"
#include "settings.h"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_puhui_number_50_4);

int ReadInitialBrightness() {
    int value = kBacklightDefaultPercent;
    if (Backlight* backlight = Board::GetInstance().GetBacklight()) {
        value = backlight->brightness();
    } else {
        Settings settings("display");
        value = settings.GetInt("brightness", kBacklightDefaultPercent);
    }
    if (value < static_cast<int>(kBacklightMinPercent)) {
        value = kBacklightMinPercent;
    }
    if (value > 100) {
        value = 100;
    }
    return value;
}

int ReadInitialVolume() {
    int volume = 70;
    if (AudioCodec* codec = Board::GetInstance().GetAudioCodec()) {
        volume = codec->output_volume();
    }
    if (volume < 0) {
        volume = 0;
    } else if (volume > 100) {
        volume = 100;
    }
    return volume;
}

void UpdatePctLabel(lv_obj_t* label, int pct) {
    if (label == nullptr) {
        return;
    }
    if (pct < 0) {
        pct = 0;
    } else if (pct > 100) {
        pct = 100;
    }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(label, buf);
}

void UpdateMinutesLabel(lv_obj_t* label, int minutes, const char* never_text) {
    if (label == nullptr) {
        return;
    }
    char buf[24];
    if (minutes <= 0) {
        std::snprintf(buf, sizeof(buf), "%s", never_text);
    } else {
        std::snprintf(buf, sizeof(buf), I18n::T("%d 分钟"), minutes);
    }
    lv_label_set_text(label, buf);
}

void ApplyBrightness(int value) {
    if (value < static_cast<int>(kBacklightMinPercent)) {
        value = kBacklightMinPercent;
    }
    Backlight* backlight = Board::GetInstance().GetBacklight();
    if (backlight != nullptr) {
        backlight->SetBrightness(static_cast<uint8_t>(value), true);
    }
}

void ApplyVolume(int volume) {
    if (volume < 0) {
        volume = 0;
    } else if (volume > 100) {
        volume = 100;
    }

    AudioCodec* codec = Board::GetInstance().GetAudioCodec();
    if (codec == nullptr || codec->output_volume() == volume) {
        return;
    }
    codec->SetOutputVolume(volume);
}

void StyleSlider(lv_obj_t* slider) {
    lv_obj_set_width(slider, LV_PCT(100));
    lv_obj_set_height(slider, 28);
    lv_obj_set_style_bg_color(slider, lv_color_hex(kColorSliderTrack), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(kColorAccent), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_pad_hor(slider, 24, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(slider, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, 10, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 10, LV_PART_INDICATOR);
    screen_swipe_back_ignore(slider, true);
}

lv_obj_t* CreateSliderRow(lv_obj_t* parent, int min_value, int max_value, int initial_value,
                          lv_event_cb_t cb, lv_obj_t** out_slider) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 52);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* slider = lv_slider_create(row);
    if (out_slider != nullptr) {
        *out_slider = slider;
    }
    StyleSlider(slider);
    lv_slider_set_range(slider, min_value, max_value);
    lv_slider_set_value(slider, initial_value, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return row;
}

void BuildSliderPanel(lv_obj_t* parent, const char* title, const char* hint, const char* range_hint,
                      int initial_value, lv_obj_t** pct_label_out, lv_obj_t** slider_out,
                      int slider_min, int slider_max, lv_event_cb_t slider_cb, int card_height) {
    lv_obj_set_style_pad_all(parent, 24, LV_PART_MAIN);
    lv_obj_set_style_pad_row(parent, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* card = lv_obj_create(parent);
    screen_strip_obj_chrome(card);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, card_height);
    lv_obj_set_style_bg_color(card, lv_color_hex(kColorCard), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 24, LV_PART_MAIN);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    screen_make_input_passive(card);

    lv_obj_t* pct = lv_label_create(card);
    if (pct_label_out != nullptr) {
        *pct_label_out = pct;
    }
    lv_obj_set_width(pct, LV_PCT(100));
    lv_label_set_long_mode(pct, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(pct, lv_color_hex(kColorValue), LV_PART_MAIN);
    lv_obj_set_style_text_font(pct, &font_puhui_number_50_4, LV_PART_MAIN);
    lv_obj_set_style_text_align(pct, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    const int value_y = card_height <= 150 ? -10 : -16;
    const int hint_y = card_height <= 150 ? -12 : -20;
    lv_obj_align(pct, LV_ALIGN_CENTER, 0, value_y);
    UpdatePctLabel(pct, initial_value);

    lv_obj_t* card_hint = lv_label_create(card);
    lv_label_set_text(card_hint, hint);
    lv_obj_set_style_text_color(card_hint, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(card_hint, &font_puhui_20_4, LV_PART_MAIN);
    lv_obj_align(card_hint, LV_ALIGN_BOTTOM_MID, 0, hint_y);

    lv_obj_t* slider_hdr = lv_obj_create(parent);
    lv_obj_remove_style_all(slider_hdr);
    lv_obj_set_width(slider_hdr, LV_PCT(100));
    lv_obj_set_height(slider_hdr, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(slider_hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(slider_hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(slider_hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* slider_title = lv_label_create(slider_hdr);
    lv_label_set_text(slider_title, title);
    lv_obj_set_style_text_color(slider_title, lv_color_hex(kColorText), LV_PART_MAIN);
    lv_obj_set_style_text_font(slider_title, &font_puhui_20_4, LV_PART_MAIN);

    lv_obj_t* range_lbl = lv_label_create(slider_hdr);
    lv_label_set_text(range_lbl, range_hint);
    lv_obj_set_style_text_color(range_lbl, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(range_lbl, &font_puhui_20_4, LV_PART_MAIN);

    CreateSliderRow(parent, slider_min, slider_max, initial_value, slider_cb, slider_out);
}
