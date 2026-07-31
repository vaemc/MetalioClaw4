#include "screen_color_test.h"

#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "pwr_key_handler.h"
#include "screen_util.h"
#include "test_screen.h"
#include "test_ui_common.h"

namespace {

constexpr const char* TAG = "ScreenColorTest";

enum class Pattern : int {
    SolidWhite = 0,
    SolidBlack,
    SolidRed,
    SolidGreen,
    SolidBlue,
    GraySteps,      // 灰阶
    ColorBars,      // 彩条
    Rainbow,        // 彩虹渐变
    Interlace,      // 交错线
    GrayGradient,   // 灰阶渐变
    Count,
};

constexpr int kPatternCount = static_cast<int>(Pattern::Count);

lv_obj_t* s_screen = nullptr;
lv_obj_t* s_canvas = nullptr;
uint8_t* s_canvas_buf = nullptr;
int s_index = 0;

inline uint16_t ToRgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void HsvToRgb(int h_deg, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (h_deg < 0) {
        h_deg = 0;
    }
    h_deg %= 360;
    const int sector = h_deg / 60;
    const int f = h_deg % 60;
    const int p = 0;
    const int q = 255 - (255 * f) / 60;
    const int t = (255 * f) / 60;
    int ri = 0, gi = 0, bi = 0;
    switch (sector) {
    case 0:
        ri = 255;
        gi = t;
        bi = p;
        break;
    case 1:
        ri = q;
        gi = 255;
        bi = p;
        break;
    case 2:
        ri = p;
        gi = 255;
        bi = t;
        break;
    case 3:
        ri = p;
        gi = q;
        bi = 255;
        break;
    case 4:
        ri = t;
        gi = p;
        bi = 255;
        break;
    default:
        ri = 255;
        gi = p;
        bi = q;
        break;
    }
    *r = static_cast<uint8_t>(ri);
    *g = static_cast<uint8_t>(gi);
    *b = static_cast<uint8_t>(bi);
}

bool AllocCanvasBuffer() {
    const size_t bytes = static_cast<size_t>(kTestPanelW) * kTestPanelH * 2;
    s_canvas_buf = static_cast<uint8_t*>(
        heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (s_canvas_buf == nullptr) {
        s_canvas_buf = static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_8BIT));
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

void FillSolid(uint32_t hex) {
    if (s_canvas == nullptr) {
        return;
    }
    lv_canvas_fill_bg(s_canvas, lv_color_hex(hex), LV_OPA_COVER);
}

void DrawVerticalBands(const uint32_t* colors, int count) {
    if (s_canvas == nullptr || colors == nullptr || count <= 0) {
        return;
    }
    lv_layer_t layer;
    lv_canvas_init_layer(s_canvas, &layer);
    for (int i = 0; i < count; ++i) {
        const int x1 = (kTestPanelW * i) / count;
        const int x2 = (kTestPanelW * (i + 1)) / count - 1;
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = lv_color_hex(colors[i]);
        dsc.bg_opa = LV_OPA_COVER;
        dsc.border_width = 0;
        dsc.radius = 0;
        lv_area_t area = {
            .x1 = static_cast<int32_t>(x1),
            .y1 = 0,
            .x2 = static_cast<int32_t>(x2),
            .y2 = static_cast<int32_t>(kTestPanelH - 1),
        };
        lv_draw_rect(&layer, &dsc, &area);
    }
    lv_canvas_finish_layer(s_canvas, &layer);
}

void DrawGraySteps() {
    constexpr int kSteps = 16;
    uint32_t colors[kSteps];
    for (int i = 0; i < kSteps; ++i) {
        const uint8_t g = static_cast<uint8_t>((i * 255) / (kSteps - 1));
        colors[i] = (static_cast<uint32_t>(g) << 16) | (static_cast<uint32_t>(g) << 8) | g;
    }
    DrawVerticalBands(colors, kSteps);
}

void DrawColorBars() {
    // 经典竖向彩条：白 / 黄 / 青 / 绿 / 品红 / 红 / 蓝 / 黑
    static constexpr uint32_t kBars[] = {
        0xFFFFFF, 0xFFFF00, 0x00FFFF, 0x00FF00,
        0xFF00FF, 0xFF0000, 0x0000FF, 0x000000,
    };
    DrawVerticalBands(kBars, static_cast<int>(sizeof(kBars) / sizeof(kBars[0])));
}

void DrawRainbow() {
    if (s_canvas_buf == nullptr) {
        return;
    }
    auto* px = reinterpret_cast<uint16_t*>(s_canvas_buf);
    for (int x = 0; x < kTestPanelW; ++x) {
        uint8_t r = 0, g = 0, b = 0;
        HsvToRgb((x * 360) / kTestPanelW, &r, &g, &b);
        const uint16_t c = ToRgb565(r, g, b);
        for (int y = 0; y < kTestPanelH; ++y) {
            px[y * kTestPanelW + x] = c;
        }
    }
    lv_obj_invalidate(s_canvas);
}

void DrawInterlace() {
    if (s_canvas == nullptr) {
        return;
    }
    // 对齐 lcd_test：黑底 + 水平/垂直 1px 交错条纹（步进 2）
    lv_canvas_fill_bg(s_canvas, lv_color_hex(0x000000), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(s_canvas, &layer);
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.border_width = 0;
    d.bg_opa = LV_OPA_COVER;

    for (int y = 0; y < kTestPanelH; y += 2) {
        d.bg_color = ((y / 2) % 2) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000);
        lv_area_t a = {
            .x1 = 0,
            .y1 = static_cast<int32_t>(y),
            .x2 = static_cast<int32_t>(kTestPanelW - 1),
            .y2 = static_cast<int32_t>(y),
        };
        lv_draw_rect(&layer, &d, &a);
    }
    for (int x = 0; x < kTestPanelW; x += 2) {
        d.bg_color = ((x / 2) % 2) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000);
        lv_area_t a = {
            .x1 = static_cast<int32_t>(x),
            .y1 = 0,
            .x2 = static_cast<int32_t>(x),
            .y2 = static_cast<int32_t>(kTestPanelH - 1),
        };
        lv_draw_rect(&layer, &d, &a);
    }
    lv_canvas_finish_layer(s_canvas, &layer);
}

void DrawGrayGradient() {
    if (s_canvas_buf == nullptr) {
        return;
    }
    auto* px = reinterpret_cast<uint16_t*>(s_canvas_buf);
    for (int x = 0; x < kTestPanelW; ++x) {
        const uint8_t g =
            static_cast<uint8_t>((x * 255) / (kTestPanelW > 1 ? (kTestPanelW - 1) : 1));
        const uint16_t c = ToRgb565(g, g, g);
        for (int y = 0; y < kTestPanelH; ++y) {
            px[y * kTestPanelW + x] = c;
        }
    }
    lv_obj_invalidate(s_canvas);
}

void ApplyPattern(int index) {
    if (s_canvas == nullptr || index < 0 || index >= kPatternCount) {
        return;
    }
    switch (static_cast<Pattern>(index)) {
    case Pattern::SolidWhite:
        FillSolid(0xFFFFFF);
        break;
    case Pattern::SolidBlack:
        FillSolid(0x000000);
        break;
    case Pattern::SolidRed:
        FillSolid(0xFF0000);
        break;
    case Pattern::SolidGreen:
        FillSolid(0x00FF00);
        break;
    case Pattern::SolidBlue:
        FillSolid(0x0000FF);
        break;
    case Pattern::GraySteps:
        DrawGraySteps();
        break;
    case Pattern::ColorBars:
        DrawColorBars();
        break;
    case Pattern::Rainbow:
        DrawRainbow();
        break;
    case Pattern::Interlace:
        DrawInterlace();
        break;
    case Pattern::GrayGradient:
        DrawGrayGradient();
        break;
    case Pattern::Count:
        break;
    }
    ESP_LOGI(TAG, "pattern index=%d", index);
}

void ReturnToTestMenu() {
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr) {
        lv_indev_wait_release(indev);
    }
    TestUiNavigateTo(TestScreen::Create);
}

void OnScreenClicked(lv_event_t* /*e*/) {
    if (s_canvas == nullptr) {
        // 无 canvas：仅在白/黑/红/绿/蓝间切换
        static constexpr uint32_t kFallback[] = {
            0xFFFFFF, 0x000000, 0xFF0000, 0x00FF00, 0x0000FF,
        };
        constexpr int kFallbackCount =
            static_cast<int>(sizeof(kFallback) / sizeof(kFallback[0]));
        if (s_index + 1 >= kFallbackCount) {
            ReturnToTestMenu();
            return;
        }
        ++s_index;
        if (s_screen != nullptr) {
            lv_obj_set_style_bg_color(s_screen, lv_color_hex(kFallback[s_index]),
                                      LV_PART_MAIN);
        }
        return;
    }
    if (s_index + 1 >= kPatternCount) {
        ESP_LOGI(TAG, "last pattern tapped, return");
        ReturnToTestMenu();
        return;
    }
    ++s_index;
    ApplyPattern(s_index);
}

void OnScreenUnloaded(lv_event_t* /*e*/) {
    s_screen = nullptr;
    s_canvas = nullptr;
    s_index = 0;
    FreeCanvasBuffer();
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
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);

    if (!AllocCanvasBuffer()) {
        // 无缓冲时退回纯色背景（仅白黑红绿蓝可点测）
        lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    } else {
        s_canvas = lv_canvas_create(scr);
        lv_canvas_set_buffer(s_canvas, s_canvas_buf, kTestPanelW, kTestPanelH,
                             LV_COLOR_FORMAT_RGB565);
        lv_obj_set_size(s_canvas, kTestPanelW, kTestPanelH);
        lv_obj_set_pos(s_canvas, 0, 0);
        lv_obj_remove_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE);
        ApplyPattern(0);
    }

    lv_obj_add_event_cb(scr, OnScreenClicked, LV_EVENT_CLICKED, nullptr);
    // 全屏点测：禁用右滑返回，避免误触；点到最后一图退出。
    screen_attach_lifecycle(scr, screen_color_lifecycle_cb);
    lv_obj_add_event_cb(scr, OnScreenUnloaded, LV_EVENT_SCREEN_UNLOADED, nullptr);
    return scr;
}
