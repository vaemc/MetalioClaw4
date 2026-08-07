/*
 * P4 JPEG hardware decode + MIPI DPI framebuffers (RGB888).
 * Reuses the already-initialized metalio panel (no BSP re-init).
 */
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/jpeg_decode.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "app_lcd.h"
#include "sdkconfig.h"

#if __has_include("config.h")
#include "config.h"
#endif

extern esp_lcd_panel_handle_t metalio_claw_4_get_panel(void);

static const char* TAG = "app_lcd";

static esp_lcd_panel_handle_t s_panel = NULL;
static jpeg_decoder_handle_t s_jpgd = NULL;
/* metalio DPI 为 RGB888；JPEG 硬件输出需 BGR 字节序，否则 R/B 对调 */
static jpeg_decode_cfg_t s_decode_cfg = {
    .output_format = JPEG_DECODE_OUT_FORMAT_RGB888,
    .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
};
static void* s_lcd_buffer[EXAMPLE_LCD_BUF_NUM] = {};
static uint8_t s_buf_index = 0;
static bool s_inited = false;

void app_lcd_draw(uint8_t* buf, uint32_t len, uint16_t width, uint16_t height) {
    (void)width;
    (void)height;
    if (!s_inited || s_panel == NULL || s_jpgd == NULL || buf == NULL || len == 0) {
        return;
    }

    static int fps_count = 0;
    static int64_t start_time = 0;
    fps_count++;
    if (fps_count == 50) {
        int64_t end_time = esp_timer_get_time();
        ESP_LOGI(TAG, "draw fps: %.1f",
                 1000000.0 / ((end_time - start_time) / 50.0));
        start_time = end_time;
        fps_count = 0;
    }

    uint32_t out_size = 0;
    esp_err_t ret =
        jpeg_decoder_process(s_jpgd, &s_decode_cfg, buf, len,
                             s_lcd_buffer[s_buf_index], EXAMPLE_LCD_BUF_LEN, &out_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "jpeg decode failed: %s", esp_err_to_name(ret));
        return;
    }

    esp_lcd_panel_draw_bitmap(s_panel, 0, 0, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES,
                              s_lcd_buffer[s_buf_index]);
    s_buf_index = (uint8_t)((s_buf_index + 1) % EXAMPLE_LCD_BUF_NUM);
}

esp_err_t app_lcd_init(void) {
    if (s_inited) {
        return ESP_OK;
    }

    s_panel = metalio_claw_4_get_panel();
    ESP_RETURN_ON_FALSE(s_panel != NULL, ESP_ERR_INVALID_STATE, TAG, "panel null");

#if EXAMPLE_LCD_H_RES != DISPLAY_WIDTH || EXAMPLE_LCD_V_RES != DISPLAY_HEIGHT
#warning "USB extend resolution differs from board DISPLAY_WIDTH/HEIGHT"
#endif

    // P4 上 JPEG 编/解码共用同一中断组；工程里硬件 JPEG 编码器已用
    // intr_priority=0 占用，再开 decoder 必须同优先级。
    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .intr_priority = 0,
        .timeout_ms = 100,
    };
    ESP_RETURN_ON_ERROR(jpeg_new_decoder_engine(&decode_eng_cfg, &s_jpgd), TAG,
                        "jpeg engine");

#if EXAMPLE_LCD_BUF_NUM == 1
    ESP_RETURN_ON_ERROR(
        esp_lcd_dpi_panel_get_frame_buffer(s_panel, 1, &s_lcd_buffer[0]), TAG,
        "get fb");
#elif EXAMPLE_LCD_BUF_NUM == 2
    ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_get_frame_buffer(
                            s_panel, 2, &s_lcd_buffer[0], &s_lcd_buffer[1]),
                        TAG, "get fb");
#else
    ESP_RETURN_ON_ERROR(
        esp_lcd_dpi_panel_get_frame_buffer(s_panel, 3, &s_lcd_buffer[0],
                                           &s_lcd_buffer[1], &s_lcd_buffer[2]),
        TAG, "get fb");
#endif

    for (int i = 0; i < EXAMPLE_LCD_BUF_NUM; ++i) {
        ESP_RETURN_ON_FALSE(s_lcd_buffer[i] != NULL, ESP_ERR_NO_MEM, TAG,
                            "fb[%d] null", i);
        // 清掉 LVGL 残留（「开启中」等），进入全黑等待首帧 USB JPEG
        memset(s_lcd_buffer[i], 0, EXAMPLE_LCD_BUF_LEN);
    }
    esp_lcd_panel_draw_bitmap(s_panel, 0, 0, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES,
                              s_lcd_buffer[0]);

    s_buf_index = 0;
    s_inited = true;
    ESP_LOGI(TAG, "LCD ready %dx%d buf=%d jpeg-hw rgb888", EXAMPLE_LCD_H_RES,
             EXAMPLE_LCD_V_RES, EXAMPLE_LCD_BUF_NUM);
    return ESP_OK;
}

void app_lcd_deinit(void) {
    if (s_jpgd != NULL) {
        jpeg_del_decoder_engine(s_jpgd);
        s_jpgd = NULL;
    }
    memset(s_lcd_buffer, 0, sizeof(s_lcd_buffer));
    s_panel = NULL;
    s_buf_index = 0;
    s_inited = false;
}
