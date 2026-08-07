#include "secondary_screen.h"

#include <cstdio>

#include "config.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "home_screen/home_screen.h"
#include "i18n.h"
#include "lvgl.h"
#include "screen_util.h"
#include "touch_feed.h"
#include "usb_extend_prefs.h"
#include "usb_extend_screen.h"

extern "C" esp_lcd_touch_handle_t metalio_claw_4_get_touch(void);

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_puhui_30_4);

namespace {

constexpr const char* TAG = "SecondaryScreen";
constexpr int kPanelSize = DISPLAY_WIDTH;
constexpr int kHeaderH = 90;
constexpr int kBackBtnSize = 72;
constexpr int kHeaderSidePad = 16;
constexpr int kTabBarH = 56;
constexpr int kSidePad = 24;
constexpr int kBtnH = 72;
constexpr int kFooterPad = 16;
constexpr int kStatusAreaH = 48;
constexpr int kFooterH = kFooterPad + kStatusAreaH + 8 + kBtnH + kFooterPad;
constexpr int kBodyH = kPanelSize - kHeaderH;
constexpr int kTabViewH = kBodyH - kFooterH;
constexpr int kContentW = kPanelSize - kSidePad * 2;

constexpr uint32_t kColorBg = 0x101418;
constexpr uint32_t kColorTabBar = 0x12151C;
constexpr uint32_t kColorText = 0xF2F4F7;
constexpr uint32_t kColorSubtle = 0x98A2B3;
constexpr uint32_t kColorBody = 0xD0D5DD;
constexpr uint32_t kColorAccent = 0x2E90FA;

void NavigateHome() {
    ESP_LOGI(TAG, "navigate home");
    lv_obj_t* old = lv_screen_active();
    lv_obj_t* home = HomeScreen::Create();
    lv_screen_load(home);
    if (old && old != home) {
        lv_obj_delete_async(old);
    }
}

void OnBackClicked(lv_event_t* e) {
    lv_event_stop_bubbling(e);
    NavigateHome();
}

struct UiState {
    lv_obj_t* screen = nullptr;
    lv_obj_t* tabview = nullptr;
    lv_obj_t* status_lbl = nullptr;
    lv_obj_t* start_btn = nullptr;
    lv_obj_t* start_lbl = nullptr;
    lv_obj_t* jpg_lbl = nullptr;
    lv_obj_t* fps_lbl = nullptr;
    lv_obj_t* frame_lbl = nullptr;
    lv_obj_t* jpg_slider = nullptr;
    lv_obj_t* fps_slider = nullptr;
    lv_obj_t* frame_slider = nullptr;
    bool starting = false;
};

UiState s_ui;

void FormatJpgLabel(char* buf, size_t n, int v) {
    std::snprintf(buf, n, "%s: %d", I18n::T("JPEG 画质"), v);
}

void FormatFpsLabel(char* buf, size_t n, int v) {
    std::snprintf(buf, n, "%s: %d", I18n::T("最大帧率"), v);
}

void FormatFrameLabel(char* buf, size_t n, int bytes) {
    std::snprintf(buf, n, "%s: %d KB", I18n::T("单帧上限"), bytes / 1024);
}

void RefreshSettingsLabels() {
    char buf[64];
    if (s_ui.jpg_lbl) {
        FormatJpgLabel(buf, sizeof(buf), usb_extend_prefs_get_jpeg_quality());
        lv_label_set_text(s_ui.jpg_lbl, buf);
    }
    if (s_ui.fps_lbl) {
        FormatFpsLabel(buf, sizeof(buf), usb_extend_prefs_get_max_fps());
        lv_label_set_text(s_ui.fps_lbl, buf);
    }
    if (s_ui.frame_lbl) {
        FormatFrameLabel(buf, sizeof(buf), usb_extend_prefs_get_frame_limit_b());
        lv_label_set_text(s_ui.frame_lbl, buf);
    }
}

void SetStatusVisible(bool visible) {
    if (s_ui.status_lbl == nullptr) {
        return;
    }
    if (visible) {
        lv_obj_remove_flag(s_ui.status_lbl, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_ui.status_lbl, LV_OBJ_FLAG_HIDDEN);
    }
}

void RefreshUi() {
    if (s_ui.status_lbl == nullptr || s_ui.start_lbl == nullptr) {
        return;
    }
    const bool running = usb_extend_screen_is_running();
    if (running) {
        lv_label_set_text(s_ui.status_lbl,
                          I18n::T("副屏运行中。短按电源键可退出；请在电脑上将显示模式设为「扩展」。"));
        SetStatusVisible(true);
        lv_label_set_text(s_ui.start_lbl, I18n::T("关闭副屏"));
    } else if (s_ui.starting) {
        lv_label_set_text(s_ui.status_lbl, I18n::T("正在开启副屏…"));
        SetStatusVisible(true);
        lv_label_set_text(s_ui.start_lbl, I18n::T("开启中…"));
    } else {
        lv_label_set_text(s_ui.status_lbl, "");
        SetStatusVisible(false);
        lv_label_set_text(s_ui.start_lbl, I18n::T("开启副屏"));
    }
    RefreshSettingsLabels();
}

void OnStoppedUi(void* /*ctx*/) {
    auto fn = [](void*) {
        if (esp_lv_adapter_lock(-1) == ESP_OK) {
            esp_lcd_touch_handle_t tp = metalio_claw_4_get_touch();
            if (tp != nullptr) {
                touch_feed_init(tp, 40);
            }
            s_ui.starting = false;
            RefreshUi();
            esp_lv_adapter_unlock();
        }
    };
    lv_async_call(fn, nullptr);
}

void StartWorker(void* /*arg*/) {
    esp_err_t err = usb_extend_screen_start();
    auto done = [](void* p) {
        const auto code = static_cast<intptr_t>(reinterpret_cast<uintptr_t>(p));
        if (esp_lv_adapter_lock(-1) == ESP_OK) {
            s_ui.starting = false;
            if (code != ESP_OK) {
                if (s_ui.status_lbl) {
                    lv_label_set_text(s_ui.status_lbl,
                                      I18n::T("开启失败，请检查 USB 线并重试"));
                    SetStatusVisible(true);
                }
                if (s_ui.start_lbl) {
                    lv_label_set_text(s_ui.start_lbl, I18n::T("开启副屏"));
                }
                RefreshSettingsLabels();
            } else {
                RefreshUi();
            }
            esp_lv_adapter_unlock();
        }
    };
    lv_async_call(done, reinterpret_cast<void*>(static_cast<uintptr_t>(err)));
    vTaskDelete(nullptr);
}

void OnStartBtn(lv_event_t* e) {
    (void)e;
    if (s_ui.starting) {
        return;
    }
    if (usb_extend_screen_is_running()) {
        usb_extend_screen_stop();
        RefreshUi();
        return;
    }
    s_ui.starting = true;
    RefreshUi();
    xTaskCreate(StartWorker, "sec_scr_start", 8192, nullptr, 5, nullptr);
}

void OnJpgSlider(lv_event_t* e) {
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int v = static_cast<int>(lv_slider_get_value(slider));
    usb_extend_prefs_set_jpeg_quality(v);
    RefreshSettingsLabels();
}

void OnFpsSlider(lv_event_t* e) {
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int v = static_cast<int>(lv_slider_get_value(slider));
    usb_extend_prefs_set_max_fps(v);
    RefreshSettingsLabels();
}

void OnFrameSlider(lv_event_t* e) {
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int kb = static_cast<int>(lv_slider_get_value(slider));
    usb_extend_prefs_set_frame_limit_b(kb * 1024);
    RefreshSettingsLabels();
}

lv_obj_t* MakeSliderRow(lv_obj_t* parent, int min_v, int max_v, int cur_v,
                        lv_event_cb_t cb, lv_obj_t** out_lbl,
                        lv_obj_t** out_slider) {
    lv_obj_t* row = lv_obj_create(parent);
    screen_strip_obj_chrome(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(row, 8, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(row);
    lv_obj_set_style_text_font(lbl, &font_puhui_20_4, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xE4E7EC), LV_PART_MAIN);
    *out_lbl = lbl;

    lv_obj_t* slider = lv_slider_create(row);
    lv_obj_set_width(slider, LV_PCT(100));
    lv_slider_set_range(slider, min_v, max_v);
    lv_slider_set_value(slider, cur_v, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, nullptr);
    screen_swipe_back_ignore(slider, true);
    *out_slider = slider;
    return row;
}

void BuildMainTab(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, kSidePad, LV_PART_MAIN);
    lv_obj_set_style_pad_row(tab, 20, LV_PART_MAIN);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_add_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(tab, LV_DIR_VER);

    auto add_step = [&](const char* index, const char* msgid, bool emphasize) {
        lv_obj_t* row = lv_obj_create(tab);
        screen_strip_obj_chrome(row);
        lv_obj_set_width(row, kContentW);
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_pad_column(row, 12, LV_PART_MAIN);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* idx = lv_label_create(row);
        lv_label_set_text(idx, index);
        lv_obj_set_style_text_font(idx, &font_puhui_20_4, LV_PART_MAIN);
        lv_obj_set_style_text_color(idx, lv_color_hex(kColorAccent), LV_PART_MAIN);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl, kContentW - 36);
        lv_label_set_text(lbl, I18n::T(msgid));
        lv_obj_set_style_text_font(lbl, &font_puhui_20_4, LV_PART_MAIN);
        lv_obj_set_style_text_color(
            lbl, lv_color_hex(emphasize ? kColorText : kColorBody), LV_PART_MAIN);
        lv_obj_set_style_text_line_space(lbl, 6, LV_PART_MAIN);
    };

    add_step("1", "使用前请先在 Windows 安装 USB 扩展屏驱动。", false);
    add_step("2",
             "请到本项目 GitHub 仓库的 secondary_screen 文件夹下载：\n"
             "xfz1986_usb_graphic_250224_rc_sign.exe",
             true);
    add_step("3",
             "安装驱动后用 USB 连接电脑，点击下方开启；在 Windows「显示设置」中选择"
             "「扩展」。支持触摸，支持播放电脑声音。",
             false);
    add_step("4", "分辨率 720×720。开启后短按电源键可退出副屏。", true);
}

void BuildSettingsTab(lv_obj_t* tab) {
    usb_extend_prefs_load();

    lv_obj_set_style_pad_all(tab, kSidePad, LV_PART_MAIN);
    lv_obj_set_style_pad_row(tab, 16, LV_PART_MAIN);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_add_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(tab, LV_DIR_VER);

    lv_obj_t* hint = lv_label_create(tab);
    lv_label_set_text(hint, I18n::T("修改后下次「开启副屏」生效"));
    lv_obj_set_style_text_font(hint, &font_puhui_20_4, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(kColorSubtle), LV_PART_MAIN);

    MakeSliderRow(tab, 1, 10, usb_extend_prefs_get_jpeg_quality(), OnJpgSlider,
                  &s_ui.jpg_lbl, &s_ui.jpg_slider);
    MakeSliderRow(tab, 1, 60, usb_extend_prefs_get_max_fps(), OnFpsSlider,
                  &s_ui.fps_lbl, &s_ui.fps_slider);
    MakeSliderRow(tab, 32, 300, usb_extend_prefs_get_frame_limit_b() / 1024,
                  OnFrameSlider, &s_ui.frame_lbl, &s_ui.frame_slider);

    RefreshSettingsLabels();
}

void BuildHeader(lv_obj_t* parent) {
    lv_obj_t* header = lv_obj_create(parent);
    screen_strip_obj_chrome(header);
    lv_obj_set_size(header, kPanelSize, kHeaderH);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* back = lv_button_create(header);
    lv_obj_remove_style_all(back);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(back, kBackBtnSize, kBackBtnSize);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, kHeaderSidePad, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        back, lv_color_hex(0xFFFFFF),
        static_cast<lv_style_selector_t>(LV_PART_MAIN | LV_STATE_PRESSED));
    lv_obj_set_style_bg_opa(
        back, LV_OPA_20,
        static_cast<lv_style_selector_t>(LV_PART_MAIN | LV_STATE_PRESSED));
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(back, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(back, OnBackClicked, LV_EVENT_CLICKED, nullptr);
    screen_swipe_back_ignore(back, true);

    lv_obj_t* back_icon = lv_image_create(back);
    lv_image_set_src(back_icon, "A:ic_app_back.spng");
    lv_obj_remove_flag(back_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(back_icon);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, I18n::T("副屏"));
    lv_obj_set_style_text_font(title, &font_puhui_30_4, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(kColorText), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_LEFT_MID,
                 kHeaderSidePad + kBackBtnSize + kHeaderSidePad, 0);
    lv_obj_remove_flag(title, LV_OBJ_FLAG_CLICKABLE);
}

void BuildFooter(lv_obj_t* parent) {
    lv_obj_t* footer = lv_obj_create(parent);
    screen_strip_obj_chrome(footer);
    lv_obj_set_size(footer, kPanelSize, kFooterH);
    lv_obj_set_pos(footer, 0, kHeaderH + kTabViewH);
    lv_obj_set_style_bg_color(footer, lv_color_hex(kColorBg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(footer, kSidePad, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(footer, kFooterPad, LV_PART_MAIN);
    lv_obj_set_style_pad_row(footer, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(footer, LV_OBJ_FLAG_CLICKABLE);

    s_ui.status_lbl = lv_label_create(footer);
    lv_obj_set_width(s_ui.status_lbl, kContentW);
    lv_label_set_long_mode(s_ui.status_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_ui.status_lbl, &font_puhui_20_4, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_ui.status_lbl, lv_color_hex(kColorSubtle),
                                LV_PART_MAIN);
    lv_obj_set_style_text_align(s_ui.status_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(s_ui.status_lbl, 4, LV_PART_MAIN);
    lv_label_set_text(s_ui.status_lbl, "");
    lv_obj_add_flag(s_ui.status_lbl, LV_OBJ_FLAG_HIDDEN);

    s_ui.start_btn = lv_button_create(footer);
    lv_obj_set_size(s_ui.start_btn, kContentW, kBtnH);
    lv_obj_set_style_radius(s_ui.start_btn, 20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.start_btn, lv_color_hex(kColorAccent),
                              LV_PART_MAIN);
    s_ui.start_lbl = lv_label_create(s_ui.start_btn);
    lv_obj_set_style_text_font(s_ui.start_lbl, &font_puhui_30_4, LV_PART_MAIN);
    lv_obj_center(s_ui.start_lbl);
    lv_obj_add_event_cb(s_ui.start_btn, OnStartBtn, LV_EVENT_CLICKED, nullptr);
    screen_swipe_back_ignore(s_ui.start_btn, true);
}

void BuildTabView(lv_obj_t* parent) {
    lv_obj_t* tv = lv_tabview_create(parent);
    s_ui.tabview = tv;
    lv_obj_set_size(tv, kPanelSize, kTabViewH);
    lv_obj_set_pos(tv, 0, kHeaderH);
    lv_tabview_set_tab_bar_position(tv, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tv, kTabBarH);

    lv_obj_set_style_bg_color(tv, lv_color_hex(kColorBg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tv, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(tv, 0, LV_PART_MAIN);

    lv_obj_t* bar = lv_tabview_get_tab_bar(tv);
    lv_obj_set_style_bg_color(bar, lv_color_hex(kColorTabBar), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(bar, lv_color_hex(kColorText), LV_PART_MAIN);
    lv_obj_set_style_text_font(bar, &font_puhui_20_4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 6, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(bar, lv_color_hex(kColorAccent),
                              static_cast<lv_style_selector_t>(LV_PART_ITEMS |
                                                               LV_STATE_CHECKED));
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER,
                            static_cast<lv_style_selector_t>(LV_PART_ITEMS |
                                                             LV_STATE_CHECKED));
    lv_obj_set_style_text_color(
        bar, lv_color_hex(kColorText),
        static_cast<lv_style_selector_t>(LV_PART_ITEMS | LV_STATE_CHECKED));

    lv_obj_t* content = lv_tabview_get_content(tv);
    screen_swipe_back_ignore(content, true);

    lv_obj_t* tab_main = lv_tabview_add_tab(tv, I18n::T("副屏"));
    BuildMainTab(tab_main);

    lv_obj_t* tab_settings = lv_tabview_add_tab(tv, I18n::T("设置"));
    BuildSettingsTab(tab_settings);
}

void OnUnload(lv_event_t* e) {
    (void)e;
    if (usb_extend_screen_is_running()) {
        usb_extend_screen_stop();
    }
    s_ui = {};
}

}  // namespace

lv_obj_t* SecondaryScreen::Create() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_size(scr, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(scr, lv_color_hex(kColorBg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    BuildHeader(scr);
    BuildTabView(scr);
    BuildFooter(scr);

    s_ui.screen = scr;
    s_ui.starting = false;
    usb_extend_screen_set_stopped_cb(OnStoppedUi, nullptr);
    RefreshUi();

    screen_attach_swipe_back(scr, NavigateHome);
    lv_obj_add_event_cb(scr, OnUnload, LV_EVENT_SCREEN_UNLOADED, nullptr);
    return scr;
}

void SecondaryScreen::LifecycleCallback(screen_lifecycle_event_t event) {
    if (event == SCREEN_LIFECYCLE_LOAD) {
        ESP_LOGI(TAG, "load");
    } else {
        ESP_LOGI(TAG, "unload");
        if (usb_extend_screen_is_running()) {
            usb_extend_screen_stop();
        }
        esp_lcd_touch_handle_t tp = metalio_claw_4_get_touch();
        if (tp != nullptr) {
            touch_feed_init(tp, 40);
        }
    }
}
