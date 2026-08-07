#pragma once

#include "lvgl.h"
#include "screen_util.h"

// USB 扩展副屏 App：提示安装 Windows IDD 驱动，一键开启后把本机当作
// Windows 扩展显示器（JPEG + 触摸 HID + UAC 扬声器）。
class SecondaryScreen {
public:
    static lv_obj_t* Create();
    static void LifecycleCallback(screen_lifecycle_event_t event);
};
