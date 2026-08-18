#include "settings_charge_tab.h"

#include <cstdint>

#include <esp_log.h>

#include "cx25601n.h"
#include "i18n.h"
#include "screen_util.h"
#include "settings.h"
#include "settings_common.h"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_puhui_30_4);

namespace {

constexpr const char* TAG = "SettingsCharge";

constexpr int kChargeNormalMa = 500;
constexpr int kChargeFastMa = 1000;
constexpr int kChargeDefaultMa = kChargeFastMa;
constexpr const char* kChargeNs = "charge";
constexpr const char* kChargeIchgKey = "ichg_ma";

struct ChargeUi {
    lv_obj_t* charge_tab = nullptr;
};
ChargeUi s_ui;

int NormalizeChargeMa(int ma) {
    if (ma == kChargeNormalMa || ma == kChargeFastMa) {
        return ma;
    }
    return kChargeDefaultMa;
}

int ReadSavedChargeMa() {
    Settings settings(kChargeNs);
    return NormalizeChargeMa(settings.GetInt(kChargeIchgKey, kChargeDefaultMa));
}

void SaveChargeMa(int ma) {
    ma = NormalizeChargeMa(ma);
    Settings settings(kChargeNs, true);
    settings.SetInt(kChargeIchgKey, ma);
}

bool ApplyChargeMa(int ma) {
    ma = NormalizeChargeMa(ma);
    if (!cx25601n_is_ready()) {
        ESP_LOGW(TAG, "CX25601N not ready, skip apply ichg=%d", ma);
        return false;
    }
    esp_err_t err = cx25601n_set_ichg_ma(static_cast<uint32_t>(ma));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set ichg=%d failed: %s", ma, esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "charge current -> %d mA", ma);
    return true;
}

void RebuildChargeTabAsync(void* /*user_data*/) {
    if (s_ui.charge_tab != nullptr) {
        SettingsChargeTab_Build(s_ui.charge_tab);
    }
}

void OnChargeModeClicked(lv_event_t* e) {
    const int ma =
        NormalizeChargeMa(static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e))));
    if (ma == ReadSavedChargeMa()) {
        ApplyChargeMa(ma);
        return;
    }
    SaveChargeMa(ma);
    ApplyChargeMa(ma);
    // 不能在 CLICKED 回调里同步删掉被点击的 card，延后重建选中态。
    lv_async_call(RebuildChargeTabAsync, nullptr);
}

}  // namespace

void SettingsChargeTab_Reset() { s_ui.charge_tab = nullptr; }

void SettingsChargeTab_Build(lv_obj_t* tab) {
    s_ui.charge_tab = tab;
    lv_obj_clean(tab);

    lv_obj_set_style_pad_all(tab, 24, LV_PART_MAIN);
    lv_obj_set_style_pad_row(tab, 16, LV_PART_MAIN);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    if (!cx25601n_is_ready()) {
        // 无芯片时不应进入本 Tab；BuildTabView 已按 ready 决定是否添加。
        return;
    }

    lv_obj_t* hint = lv_label_create(tab);
    lv_label_set_text(hint, I18n::T("选择充电电流"));
    lv_obj_set_style_text_color(hint, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &font_puhui_20_4, LV_PART_MAIN);

    const int current_ma = ReadSavedChargeMa();
    struct ChargeMode {
        int ma;
        const char* title;
        const char* subtitle;
    };
    const ChargeMode modes[] = {
        {kChargeFastMa, "快速充电", "1000 mA"},
        {kChargeNormalMa, "正常充电", "500 mA"},
    };

    for (const ChargeMode& mode : modes) {
        lv_obj_t* card = lv_obj_create(tab);
        screen_strip_obj_chrome(card);
        lv_obj_set_width(card, LV_PCT(100));
        lv_obj_set_height(card, 88);
        lv_obj_set_style_bg_color(card, lv_color_hex(kColorCard), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 20, LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
        const bool selected = (mode.ma == current_ma);
        lv_obj_set_style_border_color(card, lv_color_hex(selected ? kColorAccent : kColorCard),
                                      LV_PART_MAIN);
        lv_obj_set_style_border_opa(card, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, OnChargeModeClicked, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(mode.ma)));
        screen_swipe_back_ignore(card, true);

        lv_obj_t* name = lv_label_create(card);
        lv_label_set_text(name, I18n::T(mode.title));
        lv_obj_set_style_text_color(name, lv_color_hex(kColorText), LV_PART_MAIN);
        lv_obj_set_style_text_font(name, &font_puhui_30_4, LV_PART_MAIN);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 20, -10);

        lv_obj_t* sub = lv_label_create(card);
        lv_label_set_text(sub, I18n::T(mode.subtitle));
        lv_obj_set_style_text_color(sub, lv_color_hex(kColorSubtle), LV_PART_MAIN);
        lv_obj_set_style_text_font(sub, &font_puhui_20_4, LV_PART_MAIN);
        lv_obj_align(sub, LV_ALIGN_LEFT_MID, 20, 18);

        if (selected) {
            lv_obj_t* mark = lv_label_create(card);
            lv_label_set_text(mark, I18n::T("当前档位"));
            lv_obj_set_style_text_color(mark, lv_color_hex(kColorValue), LV_PART_MAIN);
            lv_obj_set_style_text_font(mark, &font_puhui_20_4, LV_PART_MAIN);
            lv_obj_align(mark, LV_ALIGN_RIGHT_MID, -20, 0);
        }
    }

    lv_obj_t* foot = lv_label_create(tab);
    lv_label_set_text(foot, I18n::T("充电设置会自动保存"));
    lv_obj_set_style_text_color(foot, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(foot, &font_puhui_20_4, LV_PART_MAIN);
}
