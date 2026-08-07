/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "usb_extend_defaults.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXAMPLE_LCD_H_RES (USB_EXTEND_SCREEN_HEIGHT)
#define EXAMPLE_LCD_V_RES (USB_EXTEND_SCREEN_WIDTH)
#define EXAMPLE_LCD_BUF_NUM (USB_EXTEND_LCD_BUF_COUNT)
#define EXAMPLE_LCD_BIT_PER_PIXEL (USB_EXTEND_LCD_BIT_PER_PIXEL)

#define EXAMPLE_LCD_BUF_LEN \
    (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * EXAMPLE_LCD_BIT_PER_PIXEL / 8)

esp_err_t app_lcd_init(void);
void app_lcd_deinit(void);
void app_lcd_draw(uint8_t* buf, uint32_t len, uint16_t width, uint16_t height);

#ifdef __cplusplus
}
#endif
