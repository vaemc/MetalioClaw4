#pragma once

#include "lvgl.h"

class HomeScreen {
public:
    // Create a fullscreen "home" page on a 720x720 panel.
    // Lays out app icons in a 3x3 grid (phone-style) with the icon name
    // shown directly below each icon. Returns the created LVGL screen
    // object (parent = NULL).
    static lv_obj_t* Create();
    static void RefreshStatusBar();
    // 下次进入主屏时从第一页开始（用于主题切换等场景）。
    static void ResetToFirstPage();

    // 主屏无操作自动关机时长（分钟），0 表示关闭。默认 5 分钟。
    static int GetIdleShutdownMinutes();
    static void SetIdleShutdownMinutes(int minutes);
};
