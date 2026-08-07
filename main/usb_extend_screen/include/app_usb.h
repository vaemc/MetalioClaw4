/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "sdkconfig.h"
#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_HID_TOUCH_ENABLE
typedef struct {
    uint8_t press_down;
    uint8_t index;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} __attribute__((packed)) touch_report_t;

#if CONFIG_ESP_LCD_TOUCH_MAX_POINTS != 5
#error "CONFIG_ESP_LCD_TOUCH_MAX_POINTS must be 5 for USB extend screen HID"
#endif

typedef struct {
    uint32_t report_id;
    struct {
        touch_report_t data[CONFIG_ESP_LCD_TOUCH_MAX_POINTS];
        uint8_t cnt;
    } touch_report;
} __attribute__((packed)) hid_report_t;
#endif

esp_err_t app_usb_init(void);
esp_err_t app_usb_deinit(void);

#if CONFIG_HID_TOUCH_ENABLE
void tinyusb_hid_keyboard_report(hid_report_t report);
esp_err_t app_hid_init(void);
void app_hid_deinit(void);
esp_err_t app_touch_init(void);
void app_touch_deinit(void);
#endif

#if CFG_TUD_VENDOR
esp_err_t app_vendor_init(void);
void app_vendor_deinit(void);
#endif

#if CONFIG_UAC_AUDIO_ENABLE
esp_err_t app_uac_init(void);
#endif

#ifdef __cplusplus
}
#endif
