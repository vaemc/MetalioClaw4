#pragma once

#include "esp_err.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// USB 扩展副屏（乐鑫 usb_extend_screen / xfz1986 IDD 协议）。
// 与虚拟 U 盘互斥：Start 前会强制停用 MSC 并占用 USB OTG FS。
// Start 成功后会 pause LVGL，由 JPEG 硬件解码直刷 MIPI 帧缓冲。
// 短按电源键或调用 Stop() 可退出。

typedef void (*usb_extend_screen_ui_cb_t)(void* ctx);

esp_err_t usb_extend_screen_start(void);
esp_err_t usb_extend_screen_stop(void);
bool usb_extend_screen_is_running(void);

// Stop 完成后在调用方线程之外通知 UI（可从电源键路径触发）。
void usb_extend_screen_set_stopped_cb(usb_extend_screen_ui_cb_t cb, void* ctx);

#ifdef __cplusplus
}
#endif
