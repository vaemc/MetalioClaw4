#include "screen_color_test.h"

#include "esp_log.h"
#include "pwr_key_handler.h"
#include "screen_util.h"
#include "test_screen.h"
#include "test_ui_common.h"

namespace {

constexpr const char* TAG = "ScreenColorTest";

// 依次切换；点到最后一色后返回硬件测试菜单。
constexpr uint32_t kColors[] = {
    0xFFFFFF,  // 白
    0x000000,  // 黑
    0xFF0000,  // 红
    0x00FF00,  // 绿
    0x0000FF,  // 蓝（最后一色 → 返回）
};

constexpr int kColorCount = static_cast<int>(sizeof(kColors) / sizeof(kColors[0]));

lv_obj_t* s_screen = nullptr;
int s_index = 0;

void ApplyColor(int index) {
    if (s_screen == nullptr || index < 0 || index >= kColorCount) {
        return;
    }
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(kColors[index]), LV_PART_MAIN);
}

void ReturnToTestMenu() {
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr) {
        lv_indev_wait_release(indev);
    }
    TestUiNavigateTo(TestScreen::Create);
}

void OnScreenClicked(lv_event_t* /*e*/) {
    if (s_index + 1 >= kColorCount) {
        ESP_LOGI(TAG, "last color tapped, return");
        ReturnToTestMenu();
        return;
    }
    ++s_index;
    ApplyColor(s_index);
    ESP_LOGI(TAG, "color index=%d", s_index);
}

void OnScreenUnloaded(lv_event_t* /*e*/) {
    s_screen = nullptr;
    s_index = 0;
}

void screen_color_lifecycle_cb(screen_lifecycle_event_t event) {
    PwrKey_OnScreenLifecycle("screen_color_test", event);
}

}  // namespace

lv_obj_t* ScreenColorTest::Create() {
    ESP_LOGI(TAG, "create screen color test");

    s_index = 0;
    lv_obj_t* scr = lv_obj_create(nullptr);
    s_screen = scr;
    screen_strip_obj_chrome(scr);
    lv_obj_set_size(scr, kTestPanelW, kTestPanelH);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);

    ApplyColor(0);

    lv_obj_add_event_cb(scr, OnScreenClicked, LV_EVENT_CLICKED, nullptr);
    // 全屏点测：禁用右滑返回，避免误触；靠点到最后一色退出。
    screen_attach_lifecycle(scr, screen_color_lifecycle_cb);
    lv_obj_add_event_cb(scr, OnScreenUnloaded, LV_EVENT_SCREEN_UNLOADED,
                        nullptr);
    return scr;
}
