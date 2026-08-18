#pragma once

#include "sdkconfig.h"

#if CONFIG_ESP_HOSTED_ENABLED
#include "lvgl.h"

void SettingsUpgradeTab_Build(lv_obj_t* tab);
void SettingsUpgradeTab_Reset();
#endif
