#pragma once

// WiFi 模式下未连上时拦截联网能力；4G（蜂窝）模式不拦。
// 入口级拦截（天气 / OpenClaw 等）与操作级拦截（录音→转写）共用。

// 当前为 WiFi 模式且未连接时返回 true。
bool WifiRequired_ShouldBlock();

// 在当前活动屏上弹出「未连接 WiFi」提示。
// hint_msgid：I18n 源文案（中文 msgid）；nullptr 时用默认「请先连接 WiFi 后再使用该应用」。
void WifiRequired_ShowDialog(const char* hint_msgid = nullptr);
