#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 运行时可调（NVS：ui/ues_*），开启副屏前生效并写入 USB 产品串。

void usb_extend_prefs_load(void);

int usb_extend_prefs_get_jpeg_quality(void);   // 1..10
int usb_extend_prefs_get_max_fps(void);        // 1..60
int usb_extend_prefs_get_frame_limit_b(void);  // bytes

void usb_extend_prefs_set_jpeg_quality(int quality);
void usb_extend_prefs_set_max_fps(int fps);
void usb_extend_prefs_set_frame_limit_b(int bytes);

#ifdef __cplusplus
}
#endif
