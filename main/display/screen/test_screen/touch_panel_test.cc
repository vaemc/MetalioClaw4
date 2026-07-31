#include "touch_panel_test.h"
#include "i18n.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "pwr_key_handler.h"
#include "screen_util.h"
#include "test_screen.h"
#include "test_ui_common.h"

LV_FONT_DECLARE(font_puhui_30_4);

namespace {

constexpr const char* TAG = "TouchPanelTest";

constexpr int kTargetCount = 5;
constexpr int kTargetSize = 96;
constexpr int kTargetMargin = 48;
constexpr int kBrushRadius = 28;
constexpr int kDoneBtnW = 160;
constexpr int kDoneBtnH = 64;

enum class Phase {
    TapTargets,
    Paint,
};

lv_obj_t* s_screen = nullptr;
lv_obj_t* s_hint = nullptr;
lv_obj_t* s_target = nullptr;
lv_obj_t* s_canvas = nullptr;
lv_obj_t* s_done_btn = nullptr;
uint8_t* s_canvas_buf = nullptr;
Phase s_phase = Phase::TapTargets;
int s_target_hit = 0;
int16_t s_last_x = -1;
int16_t s_last_y = -1;

void ReturnToTestMenu() {
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr) {
        lv_indev_wait_release(indev);
    }
    TestUiNavigateTo(TestScreen::Create);
}

void UpdateTapHint() {
    if (s_hint == nullptr) {
        return;
    }
    char buf[80];
    std::snprintf(buf, sizeof(buf), I18n::T("点击圆点 (%d/%d)"),
                  s_target_hit, kTargetCount);
    lv_label_set_text(s_hint, buf);
}

int RandomInRange(int lo, int hi) {
    if (hi <= lo) {
        return lo;
    }
    return lo + static_cast<int>(esp_random() % static_cast<uint32_t>(hi - lo + 1));
}

void PlaceNextTarget() {
    if (s_target == nullptr || s_screen == nullptr) {
        return;
    }
    const int max_x = kTestPanelW - kTargetSize - kTargetMargin;
    const int max_y = kTestPanelH - kTargetSize - kTargetMargin;
    const int x = RandomInRange(kTargetMargin, max_x);
    const int y = RandomInRange(kTargetMargin, max_y);
    lv_obj_set_pos(s_target, x, y);
    lv_obj_remove_flag(s_target, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_target);
    if (s_hint != nullptr) {
        lv_obj_move_foreground(s_hint);
    }
}

void PaintDotOnLayer(lv_layer_t* layer, int x, int y) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(0x22C55E);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = LV_RADIUS_CIRCLE;
    dsc.border_width = 0;

    lv_area_t area;
    area.x1 = x - kBrushRadius;
    area.y1 = y - kBrushRadius;
    area.x2 = x + kBrushRadius;
    area.y2 = y + kBrushRadius;
    lv_draw_rect(layer, &dsc, &area);
}

void PaintDot(int x, int y) {
    if (s_canvas == nullptr) {
        return;
    }
    lv_layer_t layer;
    lv_canvas_init_layer(s_canvas, &layer);
    PaintDotOnLayer(&layer, x, y);
    lv_canvas_finish_layer(s_canvas, &layer);
}

void PaintStroke(int x0, int y0, int x1, int y1) {
    if (s_canvas == nullptr) {
        return;
    }
    const int dx = x1 - x0;
    const int dy = y1 - y0;
    const int adx = dx < 0 ? -dx : dx;
    const int ady = dy < 0 ? -dy : dy;
    const int step = kBrushRadius / 2;
    const int steps = (adx > ady ? adx : ady) / (step < 1 ? 1 : step);
    const int n = steps < 1 ? 1 : steps;

    lv_layer_t layer;
    lv_canvas_init_layer(s_canvas, &layer);
    for (int i = 0; i <= n; ++i) {
        const int x = x0 + (dx * i) / n;
        const int y = y0 + (dy * i) / n;
        PaintDotOnLayer(&layer, x, y);
    }
    lv_canvas_finish_layer(s_canvas, &layer);
}

void EnterPaintPhase() {
    s_phase = Phase::Paint;
    s_last_x = -1;
    s_last_y = -1;

    if (s_target != nullptr) {
        lv_obj_add_flag(s_target, LV_OBJ_FLAG_HIDDEN);
    }

    if (s_hint != nullptr) {
        lv_label_set_text(s_hint, I18n::T("请涂满全屏，检查触摸盲区"));
        lv_obj_set_style_text_color(s_hint, lv_color_hex(0xE5E7EB), LV_PART_MAIN);
        lv_obj_align(s_hint, LV_ALIGN_TOP_MID, 0, 24);
        lv_obj_move_foreground(s_hint);
    }

    if (s_done_btn != nullptr) {
        lv_obj_remove_flag(s_done_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_done_btn);
    }

    ESP_LOGI(TAG, "enter paint phase");
}

bool AllocCanvasBuffer() {
    const size_t bytes = static_cast<size_t>(kTestPanelW) * kTestPanelH * 2;
    s_canvas_buf = static_cast<uint8_t*>(
        heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (s_canvas_buf == nullptr) {
        s_canvas_buf = static_cast<uint8_t*>(
            heap_caps_malloc(bytes, MALLOC_CAP_8BIT));
    }
    if (s_canvas_buf == nullptr) {
        ESP_LOGE(TAG, "canvas buffer alloc failed (%u bytes)",
                 static_cast<unsigned>(bytes));
        return false;
    }
    std::memset(s_canvas_buf, 0, bytes);
    return true;
}

void FreeCanvasBuffer() {
    if (s_canvas_buf != nullptr) {
        heap_caps_free(s_canvas_buf);
        s_canvas_buf = nullptr;
    }
}

void OnTargetClicked(lv_event_t* /*e*/) {
    if (s_phase != Phase::TapTargets) {
        return;
    }
    ++s_target_hit;
    UpdateTapHint();
    ESP_LOGI(TAG, "target hit %d/%d", s_target_hit, kTargetCount);
    if (s_target_hit >= kTargetCount) {
        EnterPaintPhase();
        return;
    }
    PlaceNextTarget();
}

void OnPaintEvent(lv_event_t* e) {
    if (s_phase != Phase::Paint) {
        return;
    }
    const lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = lv_event_get_indev(e);
    if (indev == nullptr) {
        indev = lv_indev_active();
    }
    if (indev == nullptr) {
        return;
    }
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    if (code == LV_EVENT_PRESSED) {
        s_last_x = pt.x;
        s_last_y = pt.y;
        PaintDot(pt.x, pt.y);
        return;
    }
    if (code == LV_EVENT_PRESSING) {
        if (s_last_x >= 0 && s_last_y >= 0) {
            PaintStroke(s_last_x, s_last_y, pt.x, pt.y);
        } else {
            PaintDot(pt.x, pt.y);
        }
        s_last_x = pt.x;
        s_last_y = pt.y;
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        s_last_x = -1;
        s_last_y = -1;
    }
}

void OnDoneClicked(lv_event_t* /*e*/) {
    ReturnToTestMenu();
}

void OnScreenUnloaded(lv_event_t* /*e*/) {
    FreeCanvasBuffer();
    s_screen = nullptr;
    s_hint = nullptr;
    s_target = nullptr;
    s_canvas = nullptr;
    s_done_btn = nullptr;
    s_phase = Phase::TapTargets;
    s_target_hit = 0;
    s_last_x = -1;
    s_last_y = -1;
}

void touch_panel_lifecycle_cb(screen_lifecycle_event_t event) {
    PwrKey_OnScreenLifecycle("touch_panel_test", event);
}

}  // namespace

lv_obj_t* TouchPanelTest::Create() {
    ESP_LOGI(TAG, "create touch panel test");

    s_phase = Phase::TapTargets;
    s_target_hit = 0;
    s_last_x = -1;
    s_last_y = -1;

    lv_obj_t* scr = lv_obj_create(nullptr);
    s_screen = scr;
    screen_strip_obj_chrome(scr);
    lv_obj_set_size(scr, kTestPanelW, kTestPanelH);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x111827), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);

    if (AllocCanvasBuffer()) {
        s_canvas = lv_canvas_create(scr);
        lv_canvas_set_buffer(s_canvas, s_canvas_buf, kTestPanelW, kTestPanelH,
                             LV_COLOR_FORMAT_RGB565);
        lv_obj_set_size(s_canvas, kTestPanelW, kTestPanelH);
        lv_obj_set_pos(s_canvas, 0, 0);
        lv_canvas_fill_bg(s_canvas, lv_color_hex(0x111827), LV_OPA_COVER);
        // 笔触由 screen 事件驱动；canvas 不抢触摸。
        screen_make_input_passive(s_canvas);
    }

    s_hint = lv_label_create(scr);
    lv_obj_set_style_text_font(s_hint, &font_puhui_30_4, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(0xE5E7EB), LV_PART_MAIN);
    lv_obj_align(s_hint, LV_ALIGN_TOP_MID, 0, 28);
    screen_make_input_passive(s_hint);
    UpdateTapHint();

    s_target = lv_obj_create(scr);
    lv_obj_remove_style_all(s_target);
    lv_obj_set_size(s_target, kTargetSize, kTargetSize);
    lv_obj_set_style_radius(s_target, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_target, lv_color_hex(0xF59E0B), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_target, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_target, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_target, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_target, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(s_target, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_target, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_target, OnTargetClicked, LV_EVENT_CLICKED, nullptr);
    screen_swipe_back_ignore(s_target, true);
    PlaceNextTarget();

    s_done_btn = lv_button_create(scr);
    lv_obj_remove_style_all(s_done_btn);
    lv_obj_set_size(s_done_btn, kDoneBtnW, kDoneBtnH);
    lv_obj_align(s_done_btn, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_set_style_radius(s_done_btn, 18, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_done_btn, lv_color_hex(0x2563EB), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_done_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_done_btn, lv_color_hex(0x1D4ED8),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_flag(s_done_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_done_btn, OnDoneClicked, LV_EVENT_CLICKED, nullptr);
    screen_swipe_back_ignore(s_done_btn, true);

    lv_obj_t* done_lbl = lv_label_create(s_done_btn);
    lv_label_set_text(done_lbl, I18n::T("完成"));
    lv_obj_set_style_text_color(done_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(done_lbl, &font_puhui_30_4, LV_PART_MAIN);
    lv_obj_center(done_lbl);
    lv_obj_remove_flag(done_lbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(scr, OnPaintEvent, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(scr, OnPaintEvent, LV_EVENT_PRESSING, nullptr);
    lv_obj_add_event_cb(scr, OnPaintEvent, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(scr, OnPaintEvent, LV_EVENT_PRESS_LOST, nullptr);

    screen_attach_lifecycle(scr, touch_panel_lifecycle_cb);
    lv_obj_add_event_cb(scr, OnScreenUnloaded, LV_EVENT_SCREEN_UNLOADED,
                        nullptr);
    return scr;
}
