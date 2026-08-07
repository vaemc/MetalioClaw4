#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// DigitalPeoplePrefs
//
// 数字人表情资源格式与 EAF 帧间隔的 NVS 持久化。
// namespace "ui"：
//   dp_fmt       string  "sjpg" | "eaf"（默认 sjpg）
//   dp_delay_ms  int32   EAF 帧间隔 ms（默认 30，范围 10~500）
// ---------------------------------------------------------------------------
namespace DigitalPeoplePrefs {

enum class EmotionFormat : int {
    Sjpg = 0,
    Eaf = 1,
};

constexpr uint32_t kDefaultFrameDelayMs = 30;
constexpr uint32_t kMinFrameDelayMs = 10;
constexpr uint32_t kMaxFrameDelayMs = 500;

EmotionFormat GetEmotionFormat();
void SetEmotionFormat(EmotionFormat format);

// 返回 ".sjpg" 或 ".eaf"
const char* GetEmotionExt();
bool UsesEaf();

uint32_t GetFrameDelayMs();
void SetFrameDelayMs(uint32_t delay_ms);

}  // namespace DigitalPeoplePrefs
