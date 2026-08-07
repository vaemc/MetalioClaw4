#include "digital_people_prefs.h"

#include <string>

#include "settings.h"

namespace DigitalPeoplePrefs {
namespace {

constexpr const char* kSettingsNs = "ui";
constexpr const char* kKeyFmt = "dp_fmt";
constexpr const char* kKeyDelayMs = "dp_delay_ms";

EmotionFormat ParseFormat(const std::string& value) {
    if (value == "eaf") {
        return EmotionFormat::Eaf;
    }
    return EmotionFormat::Sjpg;
}

const char* FormatToNvs(EmotionFormat format) {
    return format == EmotionFormat::Eaf ? "eaf" : "sjpg";
}

uint32_t ClampDelayMs(int32_t value) {
    if (value < static_cast<int32_t>(kMinFrameDelayMs)) {
        return kMinFrameDelayMs;
    }
    if (value > static_cast<int32_t>(kMaxFrameDelayMs)) {
        return kMaxFrameDelayMs;
    }
    return static_cast<uint32_t>(value);
}

}  // namespace

EmotionFormat GetEmotionFormat() {
    Settings s(kSettingsNs, false);
    return ParseFormat(s.GetString(kKeyFmt, "sjpg"));
}

void SetEmotionFormat(EmotionFormat format) {
    Settings s(kSettingsNs, true);
    s.SetString(kKeyFmt, FormatToNvs(format));
}

const char* GetEmotionExt() {
    return UsesEaf() ? ".eaf" : ".sjpg";
}

bool UsesEaf() {
    return GetEmotionFormat() == EmotionFormat::Eaf;
}

uint32_t GetFrameDelayMs() {
    Settings s(kSettingsNs, false);
    return ClampDelayMs(s.GetInt(kKeyDelayMs, static_cast<int32_t>(kDefaultFrameDelayMs)));
}

void SetFrameDelayMs(uint32_t delay_ms) {
    Settings s(kSettingsNs, true);
    s.SetInt(kKeyDelayMs, static_cast<int32_t>(ClampDelayMs(static_cast<int32_t>(delay_ms))));
}

}  // namespace DigitalPeoplePrefs
