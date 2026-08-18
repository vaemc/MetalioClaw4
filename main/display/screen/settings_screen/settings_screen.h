#pragma once

#include "lvgl.h"
#include "screen_util.h"

// 设置壳：左侧 lv_tabview + 右侧内容。各 Tab 实现见同目录 settings_*_tab.*
class SettingsScreen {
public:
    static lv_obj_t* Create();
    static void LifecycleCallback(screen_lifecycle_event_t event);
};
