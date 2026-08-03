#include "chat_screen.h"
#include "i18n.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#include "esp_log.h"
#include "lv_eaf.h"

#include "application.h"
#include "device_state.h"
#include "SdCardManager.hpp"
#include "home_screen/home_screen.h"
#include "screen_util.h"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_puhui_30_4);

namespace {

constexpr const char* TAG = "ChatScreen";

// ---------------------------------------------------------------------------
// 720x720 视觉参数
//
//   ┌───────────────────────────────────────────┐ 0
//   │ Header  [←] 聊天 待唤醒              [☰] │ 88  （表情模式可点空白显隐）
//   ├───────────────────────────────────────────┤
//   │  聊天模式：左右气泡列表                     │
//   │  表情模式：EAF 全屏居中 + 底部字幕叠加       │
//   └───────────────────────────────────────────┘ 720
// ---------------------------------------------------------------------------
constexpr int32_t kPanelW          = 720;
constexpr int32_t kPanelH          = 720;
constexpr int32_t kHeaderH         = 88;
constexpr int32_t kBackBtnSize     = 72;
constexpr int32_t kListPadH        = 18;
constexpr int32_t kListPadTop      = 14;
constexpr int32_t kListPadBottom   = 32;
constexpr int32_t kRowGap          = 12;
constexpr int32_t kBubblePadX      = 18;
constexpr int32_t kBubblePadY      = 14;
constexpr int32_t kBubbleRadius    = 18;
constexpr int32_t kSideMargin      = 8;
constexpr int32_t kMaxMessages     = 12;
constexpr int32_t kHeaderRightPad  = 16;
constexpr int32_t kMenuBtnSize     = 72;
constexpr int32_t kMenuIconSize    = 40;
constexpr int32_t kMenuPanelW      = 220;
constexpr int32_t kMenuItemH       = 64;
constexpr int32_t kMenuPanelRadius = 16;
constexpr int32_t kMenuPanelGap    = 4;  // 相对 header 底边下探

// 表情：服务器完整情绪名 → S:/sdcard/system/chat/{emotion}.eaf
constexpr const char* kEmotionDir      = "S:/sdcard/system/chat/";
constexpr const char* kEmotionPosixDir = "/sdcard/system/chat/";
constexpr const char* kEmotionExt      = ".eaf";
constexpr const char* kDefaultEmotion  = "neutral";
constexpr size_t kEmotionNameMax       = 39;  // 不含 '\0'
constexpr size_t kEmotionPathBufSize   = 96;
constexpr uint32_t kEmotionFrameDelayMs = 30;  // 与 boot_screen 一致

constexpr int32_t kEmotionBubbleBorder = 0;
constexpr int32_t kEmotionBubbleSide   = 24;
constexpr int32_t kCaptionBottom       = 36;   // 底部字幕距底边（叠加，不挤 EAF）
constexpr int32_t kEmotionBubbleMaxW   = kPanelW - kEmotionBubbleSide * 2;

constexpr uint32_t kColorBg                 = 0x0E1116;
constexpr uint32_t kColorHeaderBg           = 0x12151C;
constexpr uint32_t kColorDivider            = 0x2A2F3A;
constexpr uint32_t kColorHeaderText         = 0xFFFFFF;
constexpr uint32_t kColorHeaderBtn          = 0x2A2F3A;
constexpr uint32_t kColorHeaderBtnBorder    = 0x3B4556;
constexpr uint32_t kColorHeaderBtnText      = 0xE5E7EB;
constexpr uint32_t kColorModeSelectedBg     = 0x1E3A2F;
constexpr uint32_t kColorModeSelectedBorder = 0x34D399;
constexpr uint32_t kColorMenuPanelBg        = 0x1B2030;
constexpr uint32_t kColorLeftBubble         = 0x202736;
constexpr uint32_t kColorRightBubble        = 0x1E3A2F;
constexpr uint32_t kColorRightBubbleText    = 0xE8F5E9;
constexpr uint32_t kColorLeftBubbleText     = 0xE5E7EB;
constexpr uint32_t kColorHintText           = 0x9AA3B2;
constexpr uint32_t kColorStateIdle          = 0x9AA3B2;
constexpr uint32_t kColorStateListening     = 0x34D399;
constexpr uint32_t kColorStateSpeaking      = 0x60A5FA;
constexpr uint32_t kColorStateConnecting    = 0xFBBF24;
constexpr uint32_t kColorEmotionBubbleBg    = 0x000000;
constexpr uint32_t kColorEmotionBubbleText  = 0xFFFFFF;
constexpr lv_opa_t kEmotionBubbleBgOpa      = LV_OPA_40;

constexpr const char kEmptyHint[] = "快来和我聊天吧\n用 \"Hi 钛灵\" 唤醒我";

enum class ViewMode : uint8_t { Chat, Emotion };

struct UiState {
    lv_obj_t* screen           = nullptr;
    lv_obj_t* header           = nullptr;
    lv_obj_t* msg_list         = nullptr;
    lv_obj_t* empty_hint       = nullptr;
    lv_obj_t* status_state_lbl = nullptr;
    lv_obj_t* menu_btn         = nullptr;
    lv_obj_t* menu_mask        = nullptr;
    lv_obj_t* menu_panel       = nullptr;
    lv_obj_t* menu_item_emotion = nullptr;
    lv_obj_t* menu_item_chat    = nullptr;
    lv_obj_t* menu_item_interrupt = nullptr;
    lv_obj_t* menu_item_interrupt_lbl = nullptr;
    lv_obj_t* menu_item_clear   = nullptr;
    lv_obj_t* emotion_panel    = nullptr;
    lv_obj_t* emotion_eaf      = nullptr;
    lv_obj_t* caption_bubble   = nullptr;  // 用户/系统共用一条底部字幕
    lv_obj_t* caption_label    = nullptr;
    lv_obj_t* activation_mask  = nullptr;
    lv_timer_t* state_timer    = nullptr;
    lv_timer_t* activation_guard_timer = nullptr;
};

UiState s_ui;
DeviceState s_last_device_state = kDeviceStateUnknown;
ViewMode s_view_mode = ViewMode::Chat;
bool s_activation_blocked = false;
bool s_activation_dialog_shows_code = false;
bool s_header_visible = true;

// 当前请求的情绪名；s_applied_emotion 记录已成功 set_src 的名字，避免重复加载。
char s_current_emotion[kEmotionNameMax + 1] = "neutral";
char s_applied_emotion[kEmotionNameMax + 1] = "";
char s_emotion_path_buf[kEmotionPathBufSize];

const lv_font_t* chat_font() { return &font_puhui_30_4; }

// 仅允许 [A-Za-z0-9_-]，防止路径穿越 / 非法文件名。
bool IsSafeEmotionName(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    size_t len = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name);
         *p != '\0'; ++p, ++len) {
        if (len > kEmotionNameMax) {
            return false;
        }
        if (!(std::isalnum(*p) || *p == '_' || *p == '-')) {
            return false;
        }
    }
    return len > 0;
}

const char* NormalizeEmotionName(const char* emotion) {
    if (!IsSafeEmotionName(emotion)) {
        if (emotion != nullptr && emotion[0] != '\0') {
            ESP_LOGW(TAG, "reject unsafe emotion name: %s", emotion);
        }
        return kDefaultEmotion;
    }
    return emotion;
}

void CopyEmotionName(char* dst, size_t dst_size, const char* emotion) {
    const char* name = NormalizeEmotionName(emotion);
    std::strncpy(dst, name, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

const char* BuildEmotionPath(const char* emotion) {
    std::snprintf(s_emotion_path_buf, sizeof(s_emotion_path_buf), "%s%s%s",
                  kEmotionDir, NormalizeEmotionName(emotion), kEmotionExt);
    return s_emotion_path_buf;
}

bool EmotionFileExists(const char* emotion) {
    char path[128];
    std::snprintf(path, sizeof(path), "%s%s%s", kEmotionPosixDir,
                  NormalizeEmotionName(emotion), kEmotionExt);
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

void ApplyEmotionSrc(const char* emotion) {
    if (s_ui.emotion_eaf == nullptr) {
        return;
    }
    const char* name = NormalizeEmotionName(emotion);
    if (s_applied_emotion[0] != '\0' &&
        std::strcmp(s_applied_emotion, name) == 0) {
        return;
    }
    if (!SdCardManager::GetInstance().IsMounted() || !EmotionFileExists(name)) {
        ESP_LOGW(TAG, "emotion file missing: %s%s%s", kEmotionPosixDir, name,
                 kEmotionExt);
        return;
    }
    lv_eaf_set_src(s_ui.emotion_eaf, BuildEmotionPath(name));
    lv_eaf_set_frame_delay(s_ui.emotion_eaf, kEmotionFrameDelayMs);
    // set_src 后尺寸随原图变化，重新居中；不缩放。
    lv_obj_center(s_ui.emotion_eaf);
    std::strncpy(s_applied_emotion, name, sizeof(s_applied_emotion) - 1);
    s_applied_emotion[sizeof(s_applied_emotion) - 1] = '\0';
}

void pause_emotion_eaf() {
    if (s_ui.emotion_eaf != nullptr) {
        lv_eaf_pause(s_ui.emotion_eaf);
    }
}

void resume_emotion_eaf() {
    if (s_ui.emotion_eaf != nullptr && lv_eaf_is_loaded(s_ui.emotion_eaf)) {
        lv_eaf_resume(s_ui.emotion_eaf);
    }
}

// 立即停掉表情动画并销毁控件（不能只靠 pause + delete_async）。
void stop_emotion_eaf() {
    if (s_ui.emotion_eaf == nullptr) {
        return;
    }
    lv_eaf_pause(s_ui.emotion_eaf);
    lv_obj_delete(s_ui.emotion_eaf);
    s_ui.emotion_eaf = nullptr;
    s_applied_emotion[0] = '\0';
}

void ensure_emotion_eaf() {
    if (s_ui.emotion_eaf != nullptr || s_ui.emotion_panel == nullptr) {
        return;
    }
    if (!SdCardManager::GetInstance().IsMounted()) {
        return;
    }
    s_ui.emotion_eaf = lv_eaf_create(s_ui.emotion_panel);
    lv_obj_center(s_ui.emotion_eaf);
    screen_make_input_passive(s_ui.emotion_eaf);
}

bool is_device_activated();  // 前向声明

void schedule_voice_ui_start() {
    Application::GetInstance().ScheduleStartVoiceUiSession();
}

void schedule_voice_ui_stop() {
    Application::GetInstance().ScheduleStopVoiceUiSession();
}

// 离开聊天页：先在 LVGL 线程停 EAF，再 Schedule 停语音会话。
void chat_page_leave() {
    stop_emotion_eaf();
    schedule_voice_ui_stop();
}

// ---------------------------------------------------------------------------
// Header：设备聊天状态
// ---------------------------------------------------------------------------
bool chat_status_for_state(DeviceState state, const char** text, uint32_t* color) {
    switch (state) {
        case kDeviceStateIdle:
            *text = "待唤醒";
            *color = kColorStateIdle;
            return true;
        case kDeviceStateListening:
            *text = "聆听中";
            *color = kColorStateListening;
            return true;
        case kDeviceStateSpeaking:
            *text = "讲话中";
            *color = kColorStateSpeaking;
            return true;
        case kDeviceStateConnecting:
            *text = "连接中";
            *color = kColorStateConnecting;
            return true;
        default:
            return false;
    }
}

void chat_update_device_state_label() {
    if (s_ui.status_state_lbl == nullptr) {
        return;
    }

    const DeviceState state = Application::GetInstance().GetDeviceState();
    if (state == s_last_device_state) {
        return;
    }
    s_last_device_state = state;

    const char* text = nullptr;
    uint32_t color = kColorStateIdle;
    if (!chat_status_for_state(state, &text, &color)) {
        lv_obj_add_flag(s_ui.status_state_lbl, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_label_set_text(s_ui.status_state_lbl, I18n::T(text));
    lv_obj_set_style_text_color(s_ui.status_state_lbl, lv_color_hex(color),
                                LV_PART_MAIN);
    lv_obj_remove_flag(s_ui.status_state_lbl, LV_OBJ_FLAG_HIDDEN);
}

void on_chat_status_timer(lv_timer_t* /*timer*/) {
    chat_update_device_state_label();
}

void on_refresh_device_state_async(void* /*user_data*/) {
    chat_update_device_state_label();
}

// ---------------------------------------------------------------------------
// 列表辅助
// ---------------------------------------------------------------------------
void chat_update_empty_hint() {
    if (s_ui.empty_hint == nullptr || s_ui.msg_list == nullptr) {
        return;
    }
    const bool show = (s_view_mode == ViewMode::Chat) &&
                      (lv_obj_get_child_count(s_ui.msg_list) == 0);
    if (show) {
        lv_obj_remove_flag(s_ui.empty_hint, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_ui.empty_hint, LV_OBJ_FLAG_HIDDEN);
    }
}

void chat_scroll_to_latest(bool anim) {
    if (s_ui.msg_list == nullptr) {
        return;
    }
    const uint32_t count = lv_obj_get_child_count(s_ui.msg_list);
    if (count == 0) {
        return;
    }
    lv_obj_t* latest = lv_obj_get_child(s_ui.msg_list, count - 1);
    if (latest != nullptr) {
        lv_obj_scroll_to_view_recursive(latest, anim ? LV_ANIM_ON : LV_ANIM_OFF);
    }
}

void chat_trim_old_msgs() {
    if (s_ui.msg_list == nullptr) {
        return;
    }
    while (static_cast<int32_t>(lv_obj_get_child_count(s_ui.msg_list)) >
           kMaxMessages) {
        lv_obj_t* oldest = lv_obj_get_child(s_ui.msg_list, 0);
        if (oldest == nullptr) {
            break;
        }
        lv_obj_delete(oldest);
    }
}

void chat_clear_msg_list() {
    if (s_ui.msg_list == nullptr) {
        return;
    }
    const uint32_t count = lv_obj_get_child_count(s_ui.msg_list);
    for (int32_t i = static_cast<int32_t>(count) - 1; i >= 0; --i) {
        lv_obj_delete(lv_obj_get_child(s_ui.msg_list, i));
    }
}

// ---------------------------------------------------------------------------
// 气泡构造（文字聊天列表）
// ---------------------------------------------------------------------------
void chat_create_bubble_row(const char* text, ChatMsgDir dir) {
    if (s_ui.msg_list == nullptr || text == nullptr) {
        return;
    }

    const bool is_right = (dir == ChatMsgDir::Right);
    const lv_font_t* font = chat_font();

    lv_obj_t* row = lv_obj_create(s_ui.msg_list);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    screen_strip_obj_chrome(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(row, kRowGap, LV_PART_MAIN);

    lv_obj_t* bubble = lv_obj_create(row);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(bubble, kBubbleRadius, LV_PART_MAIN);
    lv_obj_set_style_border_width(bubble, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(bubble, kBubblePadX, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(bubble, kBubblePadY, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(bubble, 0, LV_PART_MAIN);
    lv_obj_remove_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

    const int32_t max_bubble_w = kPanelW * 72 / 100;
    int32_t text_w = lv_txt_get_width(text, std::strlen(text), font, 0);
    if (text_w < 24) {
        text_w = 24;
    }
    int32_t bubble_w = text_w + kBubblePadX * 2;
    if (bubble_w > max_bubble_w) {
        bubble_w = max_bubble_w;
    }
    lv_obj_set_width(bubble, bubble_w);

    if (is_right) {
        lv_obj_set_style_bg_color(bubble, lv_color_hex(kColorRightBubble),
                                  LV_PART_MAIN);
        lv_obj_align(bubble, LV_ALIGN_TOP_RIGHT, -kSideMargin, 0);
    } else {
        lv_obj_set_style_bg_color(bubble, lv_color_hex(kColorLeftBubble),
                                  LV_PART_MAIN);
        lv_obj_align(bubble, LV_ALIGN_TOP_LEFT, kSideMargin, 0);
    }
    lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(bubble);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, bubble_w - kBubblePadX * 2);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        label,
        lv_color_hex(is_right ? kColorRightBubbleText : kColorLeftBubbleText),
        LV_PART_MAIN);
    lv_obj_update_layout(label);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);

    screen_make_input_passive(row);
}

// ---------------------------------------------------------------------------
// 表情模式底部字幕（用户/系统共用，新消息覆盖旧内容）
// ---------------------------------------------------------------------------
void StyleCaptionBubble(lv_obj_t* bubble) {
    screen_strip_obj_chrome(bubble);
    lv_obj_remove_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bubble, lv_color_hex(kColorEmotionBubbleBg),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bubble, kEmotionBubbleBgOpa, LV_PART_MAIN);
    lv_obj_set_style_radius(bubble, kBubbleRadius, LV_PART_MAIN);
    lv_obj_set_style_border_width(bubble, kEmotionBubbleBorder, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(bubble, kBubblePadX, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(bubble, kBubblePadY, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(bubble, 0, LV_PART_MAIN);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
}

void BuildCaption(lv_obj_t* parent) {
    lv_obj_t* bubble = lv_obj_create(parent);
    StyleCaptionBubble(bubble);
    lv_obj_set_width(bubble, 100);
    lv_obj_align(bubble, LV_ALIGN_BOTTOM_MID, 0, -kCaptionBottom);
    lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* label = lv_label_create(bubble);
    lv_label_set_text(label, "");
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label, chat_font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(kColorEmotionBubbleText),
                                LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    screen_make_input_passive(bubble);

    s_ui.caption_bubble = bubble;
    s_ui.caption_label = label;
}

void ClearEmotionCaption() {
    if (s_ui.caption_label != nullptr) {
        lv_label_set_text(s_ui.caption_label, "");
    }
    if (s_ui.caption_bubble != nullptr) {
        lv_obj_add_flag(s_ui.caption_bubble, LV_OBJ_FLAG_HIDDEN);
    }
}

void ShowEmotionCaption(const char* text) {
    if (s_view_mode != ViewMode::Emotion || text == nullptr || text[0] == '\0' ||
        s_ui.caption_bubble == nullptr || s_ui.caption_label == nullptr) {
        return;
    }

    const lv_font_t* font = chat_font();
    int32_t text_w = lv_txt_get_width(text, std::strlen(text), font, 0);
    if (text_w < 32) {
        text_w = 32;
    }
    int32_t bubble_w = text_w + kBubblePadX * 2 + kEmotionBubbleBorder * 2;
    if (bubble_w > kEmotionBubbleMaxW) {
        bubble_w = kEmotionBubbleMaxW;
    }

    lv_obj_set_width(s_ui.caption_bubble, bubble_w);
    lv_obj_set_width(s_ui.caption_label,
                     bubble_w - kBubblePadX * 2 - kEmotionBubbleBorder * 2);
    lv_label_set_text(s_ui.caption_label, text);
    lv_obj_set_style_text_align(s_ui.caption_label, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN);
    lv_obj_update_layout(s_ui.caption_label);
    lv_obj_set_height(s_ui.caption_bubble, LV_SIZE_CONTENT);
    lv_obj_remove_flag(s_ui.caption_bubble, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(s_ui.caption_bubble, LV_ALIGN_BOTTOM_MID, 0, -kCaptionBottom);
    lv_obj_update_layout(s_ui.caption_bubble);
}

// ---------------------------------------------------------------------------
// 设备激活检查
// ---------------------------------------------------------------------------
bool is_device_activated() {
    return Application::GetInstance().IsDeviceActivated();
}

void log_activation_blocked() {
    auto& app = Application::GetInstance();
    ESP_LOGW(TAG, "Chat blocked: device not activated (boot_ready=%d pending=%d state=%d)",
             app.IsBootReady() ? 1 : 0,
             app.HasPendingActivation() ? 1 : 0,
             static_cast<int>(app.GetDeviceState()));
    if (app.HasPendingActivation()) {
        ESP_LOGW(TAG, "pending activation code: %s",
                 app.GetPendingActivationCode().c_str());
    }
}

void on_swipe_back();

void close_activation_blocked_dialog() {
    if (s_ui.activation_mask != nullptr) {
        lv_obj_delete(s_ui.activation_mask);
        s_ui.activation_mask = nullptr;
    }
    s_activation_dialog_shows_code = false;
}

void open_activation_blocked_dialog() {
    if (s_ui.screen == nullptr || s_ui.activation_mask != nullptr) {
        return;
    }

    auto& app = Application::GetInstance();
    const bool has_code = app.HasPendingActivation();
    s_activation_dialog_shows_code = has_code;

    constexpr int32_t kCardW = 520;
    const int32_t kCardH = has_code ? 420 : 340;
    constexpr int32_t kBackBtnW = 200;
    constexpr int32_t kBackBtnH = 72;

    lv_obj_t* mask = lv_obj_create(s_ui.screen);
    screen_strip_obj_chrome(mask);
    lv_obj_add_flag(mask, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(mask, kPanelW, kPanelH);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_set_style_bg_color(mask, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mask, LV_OPA_70, LV_PART_MAIN);
    lv_obj_remove_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
    screen_swipe_back_ignore(mask, true);
    s_ui.activation_mask = mask;

    lv_obj_t* card = lv_obj_create(mask);
    screen_strip_obj_chrome(card);
    lv_obj_set_size(card, kCardW, kCardH);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1B2030), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 24, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 28, LV_PART_MAIN);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, I18n::T("设备未激活"));
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &font_puhui_30_4, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_remove_flag(title, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* desc = lv_label_create(card);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(desc, kCardW - 56);
    lv_label_set_text(desc, I18n::T("请先完成设备激活后再使用聊天。"));
    lv_obj_set_style_text_color(desc, lv_color_hex(0x9AA3B2), LV_PART_MAIN);
    lv_obj_set_style_text_font(desc, &font_puhui_20_4, LV_PART_MAIN);
    lv_obj_set_style_text_align(desc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(desc, LV_ALIGN_CENTER, 0, has_code ? -30 : -10);
    lv_obj_remove_flag(desc, LV_OBJ_FLAG_CLICKABLE);

    if (has_code) {
        char code_buf[64];
        std::snprintf(code_buf, sizeof(code_buf), I18n::T("验证码: %s"),
                      app.GetPendingActivationCode().c_str());
        lv_obj_t* code_lbl = lv_label_create(card);
        lv_label_set_text(code_lbl, code_buf);
        lv_obj_set_style_text_color(code_lbl, lv_color_hex(0xFBBF24),
                                    LV_PART_MAIN);
        lv_obj_set_style_text_font(code_lbl, &font_puhui_30_4, LV_PART_MAIN);
        lv_obj_align(code_lbl, LV_ALIGN_BOTTOM_MID, 0, -(kBackBtnH + 24));
        lv_obj_remove_flag(code_lbl, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t* back = lv_button_create(card);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, kBackBtnW, kBackBtnH);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x2A2F3A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(back, 16, LV_PART_MAIN);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(back, [](lv_event_t* /*e*/) { on_swipe_back(); },
                        LV_EVENT_CLICKED, nullptr);
    screen_swipe_back_ignore(back, true);

    lv_obj_t* back_lbl = lv_label_create(back);
    lv_label_set_text(back_lbl, I18n::T("返回"));
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(0xE5E7EB), LV_PART_MAIN);
    lv_obj_set_style_text_font(back_lbl, &font_puhui_30_4, LV_PART_MAIN);
    lv_obj_center(back_lbl);
    lv_obj_remove_flag(back_lbl, LV_OBJ_FLAG_CLICKABLE);
}

void ensure_activation_blocked_dialog() {
    if (s_activation_blocked && s_ui.activation_mask == nullptr) {
        open_activation_blocked_dialog();
    }
}

void sync_activation_block_state() {
    auto& app = Application::GetInstance();
    const bool should_block = !is_device_activated();
    const bool has_code = app.HasPendingActivation();
    if (should_block == s_activation_blocked) {
        if (should_block) {
            // starting 时先弹出无码弹窗；激活码后到时需重建以显示验证码
            if (has_code && !s_activation_dialog_shows_code) {
                close_activation_blocked_dialog();
                open_activation_blocked_dialog();
            } else {
                ensure_activation_blocked_dialog();
            }
        }
        return;
    }
    s_activation_blocked = should_block;
    if (should_block) {
        log_activation_blocked();
        open_activation_blocked_dialog();
    } else {
        ESP_LOGI(TAG, "Chat unblocked: device activated/ready");
        close_activation_blocked_dialog();
        // LOAD 时未激活则未 Start；激活完成后在此补上。
        schedule_voice_ui_start();
    }
}

void on_activation_guard_timer(lv_timer_t* /*timer*/) {
    sync_activation_block_state();
}

bool reject_if_blocked() {
    sync_activation_block_state();
    if (!s_activation_blocked) {
        return false;
    }
    ensure_activation_blocked_dialog();
    return true;
}

// ---------------------------------------------------------------------------
// 视图模式 + 顶栏下拉菜单
// ---------------------------------------------------------------------------
enum class MenuAction : uintptr_t {
    Emotion   = 0,
    Chat      = 1,
    Clear     = 2,
    Interrupt = 3,
};

void style_menu_item(lv_obj_t* btn, bool selected) {
    if (btn == nullptr) {
        return;
    }
    lv_obj_set_style_radius(btn, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, selected ? 1 : 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        btn, lv_color_hex(selected ? kColorModeSelectedBg : kColorHeaderBtn),
        LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(kColorModeSelectedBorder),
                                  LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x3B4556),
                              LV_PART_MAIN | LV_STATE_PRESSED);
}

void set_obj_hidden(lv_obj_t* obj, bool hidden) {
    if (obj == nullptr) {
        return;
    }
    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

bool is_header_menu_open() {
    return s_ui.menu_panel != nullptr &&
           !lv_obj_has_flag(s_ui.menu_panel, LV_OBJ_FLAG_HIDDEN);
}

bool is_interrupt_enabled() {
    return Application::GetInstance().GetAecMode() != kAecOff;
}

void refresh_header_menu_selection() {
    const bool chat = (s_view_mode == ViewMode::Chat);
    style_menu_item(s_ui.menu_item_chat, chat);
    style_menu_item(s_ui.menu_item_emotion, !chat);
    // 清空只对文字气泡列表有意义，表情模式隐藏。
    set_obj_hidden(s_ui.menu_item_clear, !chat);

    const bool interrupt_on = is_interrupt_enabled();
    style_menu_item(s_ui.menu_item_interrupt, interrupt_on);
    if (s_ui.menu_item_interrupt_lbl != nullptr) {
        lv_label_set_text(s_ui.menu_item_interrupt_lbl,
                          interrupt_on ? I18n::T("打断：开") : I18n::T("打断：关"));
    }
}

void close_header_menu() {
    set_obj_hidden(s_ui.menu_mask, true);
    set_obj_hidden(s_ui.menu_panel, true);
}

void open_header_menu() {
    refresh_header_menu_selection();
    set_obj_hidden(s_ui.menu_mask, false);
    set_obj_hidden(s_ui.menu_panel, false);
    if (s_ui.menu_mask != nullptr) {
        lv_obj_move_foreground(s_ui.menu_mask);
    }
    if (s_ui.menu_panel != nullptr) {
        lv_obj_move_foreground(s_ui.menu_panel);
    }
}

void set_header_visible(bool visible) {
    s_header_visible = visible;
    set_obj_hidden(s_ui.header, !visible);
    if (!visible) {
        close_header_menu();
    } else if (s_ui.header != nullptr) {
        lv_obj_move_foreground(s_ui.header);
    }
}

void on_emotion_panel_clicked(lv_event_t* /*e*/) {
    if (s_view_mode != ViewMode::Emotion || reject_if_blocked()) {
        return;
    }
    // 菜单展开时先收起，不顺带切 header，避免误触。
    if (is_header_menu_open()) {
        close_header_menu();
        return;
    }
    set_header_visible(!s_header_visible);
}

void apply_view_mode(ViewMode mode) {
    const bool changed = (mode != s_view_mode);
    s_view_mode = mode;
    const bool chat = (mode == ViewMode::Chat);

    set_obj_hidden(s_ui.msg_list, !chat);
    set_obj_hidden(s_ui.emotion_panel, chat);

    // 切模式时恢复 header；聊天模式始终显示。
    set_header_visible(true);

    if (chat) {
        // 仅 HIDDEN 不会停 EAF timer，仍会按 frame_delay 解码占 CPU。
        pause_emotion_eaf();
    } else {
        ensure_emotion_eaf();
        if (changed || s_applied_emotion[0] == '\0') {
            ApplyEmotionSrc(s_current_emotion);
        }
        // 同名源时 ApplyEmotionSrc 会 early-return，需显式 resume。
        resume_emotion_eaf();
    }

    if (is_header_menu_open()) {
        refresh_header_menu_selection();
    }
    chat_update_empty_hint();
}

void on_clear_clicked() {
    if (reject_if_blocked()) {
        return;
    }
    ChatScreen::ClearMessages();
}

void on_interrupt_clicked() {
    if (reject_if_blocked()) {
        return;
    }
    auto& app = Application::GetInstance();
    const bool currently_on = app.GetAecMode() != kAecOff;
    const AecMode next = currently_on ? kAecOff : kAecOnDeviceSide;
    ESP_LOGI(TAG, "interrupt -> %s", currently_on ? "off" : "on");
    app.SetAecMode(next);
}

void on_menu_item_clicked(lv_event_t* e) {
    if (reject_if_blocked()) {
        close_header_menu();
        return;
    }
    const auto action = static_cast<MenuAction>(
        reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    close_header_menu();
    switch (action) {
    case MenuAction::Emotion:
        apply_view_mode(ViewMode::Emotion);
        break;
    case MenuAction::Chat:
        apply_view_mode(ViewMode::Chat);
        break;
    case MenuAction::Clear:
        on_clear_clicked();
        break;
    case MenuAction::Interrupt:
        on_interrupt_clicked();
        break;
    }
}

void on_menu_btn_clicked(lv_event_t* /*e*/) {
    if (reject_if_blocked()) {
        return;
    }
    if (is_header_menu_open()) {
        close_header_menu();
    } else {
        open_header_menu();
    }
}

void on_menu_mask_clicked(lv_event_t* /*e*/) { close_header_menu(); }

lv_obj_t* create_menu_item(lv_obj_t* parent, const char* text, MenuAction action,
                           lv_obj_t** out_label = nullptr) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_set_height(btn, kMenuItemH);
    style_menu_item(btn, false);
    lv_obj_add_event_cb(btn, on_menu_item_clicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(action)));
    screen_swipe_back_ignore(btn, true);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(kColorHeaderBtnText),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &font_puhui_20_4, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 20, 0);
    if (out_label != nullptr) {
        *out_label = lbl;
    }
    return btn;
}

void build_header_menu(lv_obj_t* parent) {
    // 全屏透明遮罩：点空白处收起菜单（在 panel 之下）
    s_ui.menu_mask = lv_obj_create(parent);
    screen_strip_obj_chrome(s_ui.menu_mask);
    lv_obj_add_flag(s_ui.menu_mask, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(s_ui.menu_mask, kPanelW, kPanelH);
    lv_obj_set_pos(s_ui.menu_mask, 0, 0);
    lv_obj_set_style_bg_opa(s_ui.menu_mask, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(s_ui.menu_mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.menu_mask, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.menu_mask, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ui.menu_mask, on_menu_mask_clicked, LV_EVENT_CLICKED,
                        nullptr);
    screen_swipe_back_ignore(s_ui.menu_mask, true);

    s_ui.menu_panel = lv_obj_create(parent);
    screen_strip_obj_chrome(s_ui.menu_panel);
    lv_obj_add_flag(s_ui.menu_panel, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(s_ui.menu_panel, kMenuPanelW, LV_SIZE_CONTENT);
    lv_obj_align(s_ui.menu_panel, LV_ALIGN_TOP_RIGHT, -kHeaderRightPad,
                 kHeaderH + kMenuPanelGap);
    lv_obj_set_style_bg_color(s_ui.menu_panel, lv_color_hex(kColorMenuPanelBg),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ui.menu_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_ui.menu_panel, kMenuPanelRadius, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_ui.menu_panel,
                                  lv_color_hex(kColorHeaderBtnBorder),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(s_ui.menu_panel, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_ui.menu_panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_ui.menu_panel, 6, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_ui.menu_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_ui.menu_panel, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(s_ui.menu_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.menu_panel, LV_OBJ_FLAG_HIDDEN);
    screen_swipe_back_ignore(s_ui.menu_panel, true);

    s_ui.menu_item_emotion =
        create_menu_item(s_ui.menu_panel, I18n::T("表情"), MenuAction::Emotion);
    s_ui.menu_item_chat =
        create_menu_item(s_ui.menu_panel, I18n::T("聊天"), MenuAction::Chat);
    s_ui.menu_item_interrupt =
        create_menu_item(s_ui.menu_panel, I18n::T("打断：关"), MenuAction::Interrupt,
                         &s_ui.menu_item_interrupt_lbl);
    s_ui.menu_item_clear =
        create_menu_item(s_ui.menu_panel, I18n::T("清空"), MenuAction::Clear);
    refresh_header_menu_selection();
}

// ---------------------------------------------------------------------------
// 屏幕导航
// ---------------------------------------------------------------------------
void on_swipe_back() {
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr) {
        lv_indev_wait_release(indev);
    }
    chat_page_leave();

    lv_obj_t* old_scr = lv_screen_active();
    lv_obj_t* home = HomeScreen::Create();
    lv_screen_load(home);
    if (old_scr != nullptr && old_scr != home) {
        lv_obj_delete_async(old_scr);
    }
}

void delete_timer(lv_timer_t*& timer) {
    if (timer != nullptr) {
        lv_timer_delete(timer);
        timer = nullptr;
    }
}

void on_screen_unloaded(lv_event_t* e) {
    // 仅清理当前实例：Create 重入后，旧屏异步 delete 触发的 UNLOADED
    // 不能把新屏的静态引用清掉。
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (target != s_ui.screen) {
        return;
    }
    chat_page_leave();
    delete_timer(s_ui.state_timer);
    delete_timer(s_ui.activation_guard_timer);
    s_ui = UiState{};
    s_activation_blocked = false;
    s_activation_dialog_shows_code = false;
    s_header_visible = true;
    s_last_device_state = kDeviceStateUnknown;
    s_applied_emotion[0] = '\0';
}

// ---------------------------------------------------------------------------
// UI 组装
// ---------------------------------------------------------------------------
void build_header(lv_obj_t* parent) {
    lv_obj_t* header = lv_obj_create(parent);
    s_ui.header = header;
    screen_strip_obj_chrome(header);
    // FLOATING：表情全屏时叠在 EAF 之上，可独立显隐。
    lv_obj_add_flag(header, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(header, kPanelW, kHeaderH);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(kColorHeaderBg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* divider = lv_obj_create(header);
    screen_strip_obj_chrome(divider);
    lv_obj_set_size(divider, kPanelW, 1);
    lv_obj_align(divider, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(divider, lv_color_hex(kColorDivider), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN);
    screen_make_input_passive(divider);

    lv_obj_t* back = lv_button_create(header);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, kBackBtnSize, kBackBtnSize);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(back, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(back, LV_OPA_20, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(back, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(back, [](lv_event_t* /*e*/) { on_swipe_back(); },
                        LV_EVENT_CLICKED, nullptr);
    screen_swipe_back_ignore(back, true);

    lv_obj_t* back_icon = lv_image_create(back);
    lv_image_set_src(back_icon, "A:ic_app_back.spng");
    lv_obj_remove_flag(back_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(back_icon);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, I18n::T("聊天"));
    lv_obj_set_style_text_color(title, lv_color_hex(kColorHeaderText),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &font_puhui_30_4, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 16 + kBackBtnSize + 12, 0);

    s_ui.status_state_lbl = lv_label_create(header);
    lv_label_set_text(s_ui.status_state_lbl, I18n::T("待唤醒"));
    lv_obj_set_style_text_font(s_ui.status_state_lbl, &font_puhui_20_4,
                               LV_PART_MAIN);
    lv_obj_set_style_text_color(s_ui.status_state_lbl,
                                lv_color_hex(kColorStateIdle), LV_PART_MAIN);
    lv_obj_align_to(s_ui.status_state_lbl, title, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
    screen_make_input_passive(s_ui.status_state_lbl);

    s_last_device_state = kDeviceStateUnknown;
    chat_update_device_state_label();
    s_ui.state_timer = lv_timer_create(on_chat_status_timer, 500, nullptr);

    s_ui.menu_btn = lv_button_create(header);
    lv_obj_remove_style_all(s_ui.menu_btn);
    lv_obj_set_size(s_ui.menu_btn, kMenuBtnSize, kMenuBtnSize);
    lv_obj_align(s_ui.menu_btn, LV_ALIGN_RIGHT_MID, -kHeaderRightPad, 0);
    lv_obj_set_style_bg_opa(s_ui.menu_btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.menu_btn, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(s_ui.menu_btn, LV_OPA_20,
                            LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(s_ui.menu_btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_ui.menu_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_ui.menu_btn, on_menu_btn_clicked, LV_EVENT_CLICKED,
                        nullptr);
    screen_swipe_back_ignore(s_ui.menu_btn, true);

    lv_obj_t* menu_icon = lv_image_create(s_ui.menu_btn);
    lv_image_set_src(menu_icon, "A:ic_app_chat_menu.spng");
    lv_image_set_inner_align(menu_icon, LV_IMAGE_ALIGN_CENTER);
    lv_obj_set_size(menu_icon, kMenuIconSize, kMenuIconSize);
    lv_obj_remove_flag(menu_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(menu_icon);
}

void build_message_list(lv_obj_t* parent) {
    s_ui.msg_list = lv_obj_create(parent);
    lv_obj_set_size(s_ui.msg_list, kPanelW, kPanelH - kHeaderH);
    lv_obj_set_pos(s_ui.msg_list, 0, kHeaderH);
    screen_strip_obj_chrome(s_ui.msg_list);
    lv_obj_set_style_bg_opa(s_ui.msg_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_left(s_ui.msg_list, kListPadH, LV_PART_MAIN);
    lv_obj_set_style_pad_right(s_ui.msg_list, kListPadH, LV_PART_MAIN);
    lv_obj_set_style_pad_top(s_ui.msg_list, kListPadTop, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(s_ui.msg_list, kListPadBottom, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(s_ui.msg_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(s_ui.msg_list, LV_DIR_VER);
    lv_obj_set_flex_flow(s_ui.msg_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_ui.msg_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    s_ui.empty_hint = lv_label_create(parent);
    lv_label_set_text(s_ui.empty_hint, I18n::T(kEmptyHint));
    lv_obj_set_width(s_ui.empty_hint, kPanelW * 80 / 100);
    lv_label_set_long_mode(s_ui.empty_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_ui.empty_hint, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(s_ui.empty_hint, &font_puhui_30_4, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_ui.empty_hint, lv_color_hex(kColorHintText),
                                LV_PART_MAIN);
    lv_obj_align(s_ui.empty_hint, LV_ALIGN_TOP_MID, 0,
                 kHeaderH + (kPanelH - kHeaderH) / 2 - 50);
    screen_make_input_passive(s_ui.empty_hint);
}

void build_emotion_panel(lv_obj_t* parent) {
    s_ui.emotion_panel = lv_obj_create(parent);
    screen_strip_obj_chrome(s_ui.emotion_panel);
    // 全屏铺底；header 以 FLOATING 叠在上面。
    lv_obj_set_size(s_ui.emotion_panel, kPanelW, kPanelH);
    lv_obj_set_pos(s_ui.emotion_panel, 0, 0);
    lv_obj_set_style_bg_color(s_ui.emotion_panel, lv_color_hex(kColorBg),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ui.emotion_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(s_ui.emotion_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.emotion_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ui.emotion_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ui.emotion_panel, on_emotion_panel_clicked,
                        LV_EVENT_CLICKED, nullptr);

    if (!SdCardManager::GetInstance().IsMounted()) {
        ESP_LOGW(TAG, "chat emotion: SD card not mounted");
        lv_obj_t* hint = lv_label_create(s_ui.emotion_panel);
        lv_label_set_text(
            hint, I18n::T("未检测到 SD 卡\n\n请将聊天表情资源放入 SD 卡\n"
                          "system/chat/ 目录"));
        lv_obj_set_width(hint, kPanelW - 80);
        lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_color(hint, lv_color_hex(kColorHintText),
                                    LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, chat_font(), LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);
        screen_make_input_passive(hint);
    } else {
        s_ui.emotion_eaf = lv_eaf_create(s_ui.emotion_panel);
        // 保持原图像素尺寸；set_src / frame_delay 留给 ApplyEmotionSrc。
        lv_obj_center(s_ui.emotion_eaf);
        screen_make_input_passive(s_ui.emotion_eaf);
        // 真正加载留给 apply_view_mode，避免 Create 时重复 set_src
    }

    BuildCaption(s_ui.emotion_panel);
}

}  // namespace

// ===========================================================================
// 公共接口
// ===========================================================================
lv_obj_t* ChatScreen::Create() {
    // 防御：异常重入时先清掉旧静态引用（对象本身由 LVGL 异步删除）。
    if (s_ui.screen != nullptr) {
        ESP_LOGW(TAG, "Create while previous screen still active, resetting refs");
        chat_page_leave();
        delete_timer(s_ui.state_timer);
        delete_timer(s_ui.activation_guard_timer);
        s_ui = UiState{};
        s_applied_emotion[0] = '\0';
        s_header_visible = true;
    }

    s_activation_blocked = !is_device_activated();
    if (s_activation_blocked) {
        log_activation_blocked();
    }

    lv_obj_t* scr = lv_obj_create(nullptr);
    s_ui.screen = scr;
    screen_strip_obj_chrome(scr);
    lv_obj_set_size(scr, kPanelW, kPanelH);
    lv_obj_set_style_bg_color(scr, lv_color_hex(kColorBg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_header(scr);
    build_message_list(scr);
    build_emotion_panel(scr);
    build_header_menu(scr);
    apply_view_mode(s_view_mode);

    if (s_activation_blocked) {
        open_activation_blocked_dialog();
    }
    // 启动完成前进入时保持轮询，boot_ready 后自动撤掉拦截
    s_ui.activation_guard_timer =
        lv_timer_create(on_activation_guard_timer, 1000, nullptr);

    screen_attach_swipe_back(scr, on_swipe_back);
    lv_obj_add_event_cb(scr, on_screen_unloaded, LV_EVENT_SCREEN_UNLOADED,
                        nullptr);
    lv_obj_add_event_cb(
        scr,
        [](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
                sync_activation_block_state();
            }
        },
        LV_EVENT_SCREEN_LOADED, nullptr);
    return scr;
}

void ChatScreen::LifecycleCallback(screen_lifecycle_event_t event) {
    if (event == SCREEN_LIFECYCLE_LOAD) {
        if (!is_device_activated()) {
            ESP_LOGW(TAG, "load: chat_screen blocked (device not activated)");
            log_activation_blocked();
        } else {
            ESP_LOGI(TAG, "load: chat_screen -> schedule voice UI start");
            schedule_voice_ui_start();
        }
        RefreshDeviceState();
    } else {
        ESP_LOGI(TAG, "unload: chat_screen -> schedule voice UI stop");
        chat_page_leave();
    }
}

bool ChatScreen::IsActive() {
    return s_ui.screen != nullptr && s_ui.msg_list != nullptr;
}

void ChatScreen::RefreshDeviceState() {
    if (!IsActive()) {
        return;
    }
    lv_async_call(on_refresh_device_state_async, nullptr);
}

void ChatScreen::AddMessage(const char* text, ChatMsgDir dir) {
    if (text == nullptr || text[0] == '\0' || !IsActive() ||
        s_activation_blocked) {
        return;
    }

    chat_create_bubble_row(text, dir);
    chat_trim_old_msgs();
    chat_update_empty_hint();
    if (s_view_mode == ViewMode::Chat) {
        lv_obj_update_layout(s_ui.msg_list);
        chat_scroll_to_latest(true);
    } else {
        ShowEmotionCaption(text);
    }
}

void ChatScreen::ClearMessages() {
    chat_clear_msg_list();
    ClearEmotionCaption();
    chat_update_empty_hint();
}

void ChatScreen::SetEmotion(const char* emotion) {
    CopyEmotionName(s_current_emotion, sizeof(s_current_emotion), emotion);
    ESP_LOGI(TAG, "SetEmotion -> %s", s_current_emotion);

    // 必须在前台且表情模式才加载；否则 Idle 的 SetEmotion(neutral) 会在
    // 退出过程中把已 pause 的 EAF 再次 set_src/resume，占满 CPU。
    if (IsActive() && s_view_mode == ViewMode::Emotion) {
        ApplyEmotionSrc(s_current_emotion);
    }
}
