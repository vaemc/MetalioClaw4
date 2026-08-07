#pragma once

#include "lvgl.h"

// ---------------------------------------------------------------------------
// DigitalPeopleSettingsScreen
//
// 数字人设置：选择表情资源格式（EAF / SJPG）与 EAF 帧间隔，写入 NVS。
// 仅左上角返回数字人主界面（不启用右滑返回，避免拖滑块误触）。
// ---------------------------------------------------------------------------
class DigitalPeopleSettingsScreen {
public:
    static lv_obj_t* Create();
};
