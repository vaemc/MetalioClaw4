#include "app_usb.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb_descriptors.h"

extern esp_lcd_touch_handle_t metalio_claw_4_get_touch(void);

static const char* TAG = "app_touch";
static esp_lcd_touch_handle_t s_tp = NULL;
static TaskHandle_t s_task = NULL;
static volatile bool s_run = false;

static void app_touch_task(void* arg) {
    (void)arg;
    uint8_t touchpad_cnt = 0;
    bool send_press = false;
    while (s_run) {
        if (s_tp == NULL) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        esp_lcd_touch_read_data(s_tp);
        esp_lcd_touch_point_data_t touch_points[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {
            0};
        if (esp_lcd_touch_get_data(s_tp, touch_points, &touchpad_cnt,
                                   CONFIG_ESP_LCD_TOUCH_MAX_POINTS) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        hid_report_t report = {0};
        if (touchpad_cnt > 0) {
            report.report_id = REPORT_ID_TOUCH;
            for (int i = 0; i < touchpad_cnt; i++) {
                report.touch_report.data[i].index = touch_points[i].track_id;
                report.touch_report.data[i].press_down = 1;
                report.touch_report.data[i].x = touch_points[i].x;
                report.touch_report.data[i].y = touch_points[i].y;
                report.touch_report.data[i].width = touch_points[i].strength;
                report.touch_report.data[i].height = touch_points[i].strength;
            }
            report.touch_report.cnt = touchpad_cnt;
#if CFG_TUD_HID
            tinyusb_hid_keyboard_report(report);
#endif
            send_press = true;
        } else if (send_press) {
            send_press = false;
            report.report_id = REPORT_ID_TOUCH;
#if CFG_TUD_HID
            tinyusb_hid_keyboard_report(report);
#endif
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t app_touch_init(void) {
    s_tp = metalio_claw_4_get_touch();
    if (s_tp == NULL) {
        ESP_LOGW(TAG, "touch handle null, HID touch disabled");
        return ESP_OK;
    }
    s_run = true;
    if (xTaskCreate(app_touch_task, "app_touch_task", 4096, NULL,
                    CONFIG_TOUCH_TASK_PRIORITY, &s_task) != pdPASS) {
        s_run = false;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "touch HID task started");
    return ESP_OK;
}

void app_touch_deinit(void) {
    s_run = false;
    for (int i = 0; i < 50 && s_task != NULL; ++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (s_task != NULL) {
        vTaskDelete(s_task);
        s_task = NULL;
    }
    s_tp = NULL;
}
