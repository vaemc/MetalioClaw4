#include "touch_feed.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace {

constexpr const char* kTag = "TouchFeed";
constexpr uint32_t kDefaultPeriodMs = 40;
constexpr uint32_t kMaxBackoffMs = 400;

esp_lcd_touch_handle_t s_handle = nullptr;
SemaphoreHandle_t s_mutex = nullptr;
TaskHandle_t s_task = nullptr;
volatile bool s_run = false;
uint32_t s_period_ms = kDefaultPeriodMs;

struct TouchSnapshot {
    bool pressed = false;
    int16_t x = 0;
    int16_t y = 0;
};

TouchSnapshot s_snap;

#if TOUCH_FEED_DEBUG
bool s_log_was_pressed = false;
int s_log_last_x = -1;
int s_log_last_y = -1;

void LogSnapshotIfChanged(const TouchSnapshot& next) {
    if (!next.pressed) {
        if (s_log_was_pressed) {
            ESP_LOGW(kTag, "chip: released");
            s_log_was_pressed = false;
            s_log_last_x = -1;
            s_log_last_y = -1;
        }
        return;
    }

    const int dx = (s_log_last_x >= 0) ? (next.x - s_log_last_x) : 0;
    const int dy = (s_log_last_y >= 0) ? (next.y - s_log_last_y) : 0;
    const bool moved = !s_log_was_pressed || dx != 0 || dy != 0;
    if (moved) {
        ESP_LOGW(kTag, "chip: p0=(%d,%d) d=(%+d,%+d)%s", next.x, next.y, dx,
                 dy, s_log_was_pressed ? "" : " [down]");
    }

    s_log_was_pressed = true;
    s_log_last_x = next.x;
    s_log_last_y = next.y;
}
#endif

// 返回 true 表示本轮 I2C 读成功。
bool UpdateSnapshotFromChip() {
    if (s_handle == nullptr) {
        return false;
    }

    if (esp_lcd_touch_read_data(s_handle) != ESP_OK) {
        return false;
    }

    esp_lcd_touch_point_data_t points[1] = {};
    uint8_t cnt = 0;
    if (esp_lcd_touch_get_data(s_handle, points, &cnt, 1) != ESP_OK) {
        return false;
    }

    TouchSnapshot next = s_snap;
    if (cnt > 0) {
        next.pressed = true;
        next.x = static_cast<int16_t>(points[0].x);
        next.y = static_cast<int16_t>(points[0].y);
    } else {
        next.pressed = false;
    }

    if (s_mutex != nullptr &&
        xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_snap = next;
        xSemaphoreGive(s_mutex);
    }

#if TOUCH_FEED_DEBUG
    LogSnapshotIfChanged(next);
#endif
    return true;
}

void ReaderTask(void* /*arg*/) {
    const uint32_t period_ms = s_period_ms;
    uint32_t consecutive_err = 0;
#if TOUCH_FEED_DEBUG
    ESP_LOGI(kTag, "reader started, period=%u ms", period_ms);
#endif

    while (s_run) {
        if (UpdateSnapshotFromChip()) {
            consecutive_err = 0;
            vTaskDelay(pdMS_TO_TICKS(period_ms));
        } else {
            // 总线异常时拉长间隔，避免 clear-bus 失败后继续狂刷
            if (consecutive_err < 8) {
                consecutive_err++;
            }
            uint32_t backoff = period_ms << (consecutive_err > 3 ? 3 : consecutive_err);
            if (backoff > kMaxBackoffMs) {
                backoff = kMaxBackoffMs;
            }
            if (consecutive_err == 1 || consecutive_err == 4 || consecutive_err == 8) {
                ESP_LOGW(kTag, "I2C read fail, backoff %u ms (n=%u)", backoff,
                         consecutive_err);
            }
            vTaskDelay(pdMS_TO_TICKS(backoff));
        }
    }

    s_task = nullptr;
    vTaskDelete(nullptr);
}

void IndevReadCb(lv_indev_t* indev, lv_indev_data_t* data) {
    (void)indev;
    if (data == nullptr) {
        return;
    }

    TouchSnapshot snap;
    if (s_mutex != nullptr &&
        xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        snap = s_snap;
        xSemaphoreGive(s_mutex);
    }

    data->point.x = snap.x;
    data->point.y = snap.y;
    data->state =
        snap.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

}  // namespace

void touch_feed_init(esp_lcd_touch_handle_t handle, uint32_t period_ms) {
    touch_feed_stop();

    s_handle = handle;
    s_period_ms = (period_ms == 0) ? kDefaultPeriodMs : period_ms;

    if (s_mutex == nullptr) {
        s_mutex = xSemaphoreCreateMutex();
    }
    if (s_mutex == nullptr) {
        ESP_LOGE(kTag, "mutex create failed");
        return;
    }

    {
        TouchSnapshot cleared;
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            s_snap = cleared;
            xSemaphoreGive(s_mutex);
        }
    }
#if TOUCH_FEED_DEBUG
    s_log_was_pressed = false;
    s_log_last_x = -1;
    s_log_last_y = -1;
#endif

    s_run = true;
    if (xTaskCreate(ReaderTask, "touch_feed", 4096, nullptr, 5, &s_task) !=
        pdPASS) {
        s_run = false;
        s_task = nullptr;
        ESP_LOGE(kTag, "xTaskCreate failed");
        return;
    }
}

void touch_feed_attach_indev(lv_indev_t* indev) {
    if (indev == nullptr) {
        ESP_LOGW(kTag, "attach_indev: null indev");
        return;
    }
    lv_indev_set_read_cb(indev, IndevReadCb);
}

void touch_feed_stop() {
    if (s_task != nullptr) {
        s_run = false;
        for (int i = 0; i < 50 && s_task != nullptr; ++i) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (s_task != nullptr) {
            vTaskDelete(s_task);
            s_task = nullptr;
        }
    }
    s_run = false;
    s_handle = nullptr;
}
