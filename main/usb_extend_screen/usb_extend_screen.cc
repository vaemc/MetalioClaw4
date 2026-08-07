#include "usb_extend_screen.h"

#include <atomic>
#include <mutex>

#include "app_lcd.h"
#include "app_usb.h"
#include "application.h"
#include "display/touch_feed.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb_extend_defaults.h"
#include "usb_extend_prefs.h"
#include "usb_virtual_disk.h"

static const char* TAG = "UsbExtend";

namespace {

std::atomic<bool> s_running{false};
std::mutex s_mu;
bool s_lvgl_paused = false;
bool s_touch_stopped = false;
usb_extend_screen_ui_cb_t s_stopped_cb = nullptr;
void* s_stopped_ctx = nullptr;

void NotifyStopped() {
    usb_extend_screen_ui_cb_t cb = nullptr;
    void* ctx = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_mu);
        cb = s_stopped_cb;
        ctx = s_stopped_ctx;
    }
    if (cb) {
        cb(ctx);
    }
}

void WaitVirtualDiskIdle() {
    auto& vd = UsbVirtualDisk::GetInstance();
    if (!vd.IsSupported()) {
        return;
    }
    if (vd.IsGadgetActive() || vd.IsBusy()) {
        ESP_LOGI(TAG, "disabling virtual U-disk before extend screen");
        vd.DisableIfActive();
        for (int i = 0; i < 80; ++i) {
            if (!vd.IsGadgetActive() && !vd.IsBusy()) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

}  // namespace

extern "C" void usb_extend_screen_set_stopped_cb(usb_extend_screen_ui_cb_t cb,
                                                 void* ctx) {
    std::lock_guard<std::mutex> lock(s_mu);
    s_stopped_cb = cb;
    s_stopped_ctx = ctx;
}

extern "C" bool usb_extend_screen_is_running(void) {
    return s_running.load(std::memory_order_relaxed);
}

extern "C" esp_err_t usb_extend_screen_start(void) {
    if (s_running.load(std::memory_order_relaxed)) {
        return ESP_OK;
    }

    WaitVirtualDiskIdle();
    usb_extend_prefs_load();

    // 暂停语音会话，避免与 UAC 抢 I2S
    Application::GetInstance().StopListening();
    Application::GetInstance().SetActivationSuspended(true);

    if (esp_lv_adapter_is_initialized()) {
        if (esp_lv_adapter_pause(-1) == ESP_OK) {
            s_lvgl_paused = true;
        } else {
            ESP_LOGW(TAG, "esp_lv_adapter_pause failed");
        }
    }

    // 停掉 LVGL 触控读任务，改由 HID 任务独占 GT911
    touch_feed_stop();
    s_touch_stopped = true;

    esp_err_t err = app_lcd_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "app_lcd_init: %s", esp_err_to_name(err));
        goto fail;
    }

    err = app_usb_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "app_usb_init: %s", esp_err_to_name(err));
        goto fail;
    }

    s_running.store(true, std::memory_order_relaxed);
    ESP_LOGI(TAG, "USB extend screen started (%dx%d) jpg=%d fps=%d limit=%d",
             USB_EXTEND_SCREEN_HEIGHT, USB_EXTEND_SCREEN_WIDTH,
             usb_extend_prefs_get_jpeg_quality(), usb_extend_prefs_get_max_fps(),
             usb_extend_prefs_get_frame_limit_b());
    return ESP_OK;

fail:
    (void)usb_extend_screen_stop();
    return err;
}

extern "C" esp_err_t usb_extend_screen_stop(void) {
    const bool was_running = s_running.exchange(false, std::memory_order_relaxed);

    (void)app_usb_deinit();
    app_lcd_deinit();

    if (s_touch_stopped) {
        // 由 secondary_screen / 主页重新 Create 时会 touch_feed_init；
        // 这里尽量恢复：若板级仍持有 touch handle，由 UI 侧再 init。
        s_touch_stopped = false;
    }

    if (s_lvgl_paused) {
        esp_lv_adapter_resume();
        s_lvgl_paused = false;
    }

    Application::GetInstance().SetActivationSuspended(false);

    if (was_running) {
        ESP_LOGI(TAG, "USB extend screen stopped");
        NotifyStopped();
    }
    return ESP_OK;
}
