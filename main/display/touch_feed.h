#pragma once

#include "esp_lcd_touch.h"
#include "lvgl.h"

// 芯片坐标 trace 日志：1 开启，0 关闭（默认）。
#ifndef TOUCH_FEED_DEBUG
#define TOUCH_FEED_DEBUG 0
#endif

// 单一读源：独立任务读触摸芯片，LVGL indev read_cb 只读 mutex 保护的快照。
// 若 touch 配置了 int_gpio（非 NC），init 时注册 ISR：空闲等中断，按下期间
// 仍按 period_ms 轮询以保证滑动连续；无 INT 脚则退回纯周期轮询。
void touch_feed_init(esp_lcd_touch_handle_t handle, uint32_t period_ms);
void touch_feed_attach_indev(lv_indev_t* indev);
void touch_feed_stop();
