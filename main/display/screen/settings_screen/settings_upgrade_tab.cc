#include "settings_upgrade_tab.h"

#if CONFIG_ESP_HOSTED_ENABLED

#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstring>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "board.h"
#include "esp_hosted.h"
#include "esp_hosted_api_types.h"
#include "esp_hosted_ota.h"
#include "i18n.h"
#include "screen_util.h"
#include "settings_common.h"

#include <esp_netif.h>

extern "C" esp_err_t rpc_get_coprocessor_fwversion(esp_hosted_coprocessor_fwver_t* ver_info);

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_puhui_30_4);

namespace {

constexpr const char* TAG = "SettingsUpgrade";

constexpr uint32_t kC5TargetMajor = 2;
constexpr uint32_t kC5TargetMinor = 12;
constexpr uint32_t kC5TargetPatch = 12;
constexpr const char* kC5FwUrl =
    "https://cloudzaoai.oss-rg-china-mainland.aliyuncs.com/c5_hosted_2.12.12.bin";
constexpr size_t kC5OtaChunkSize = 1400;

std::atomic_bool s_c5_ota_busy{false};
std::atomic_bool s_version_fetching{false};
std::atomic_uint32_t s_ui_gen{0};

struct C5Ui {
    lv_obj_t* tab = nullptr;
    lv_obj_t* tabview = nullptr;
    lv_obj_t* tab_btn = nullptr;
    int32_t tab_index = -1;
    lv_obj_t* version_label = nullptr;
    lv_obj_t* target_prefix_label = nullptr;
    lv_obj_t* target_ver_label = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* update_btn = nullptr;
    lv_obj_t* progress_bar = nullptr;
    lv_obj_t* progress_label = nullptr;
    bool version_ready = false;
    bool up_to_date = false;
};
C5Ui s_ui;

struct C5FwVersion {
    bool ok = false;
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
};

bool IsC5TargetVersion(const C5FwVersion& v) {
    return v.ok && v.major == kC5TargetMajor && v.minor == kC5TargetMinor &&
           v.patch == kC5TargetPatch;
}

bool IsNetworkReadyForDownload() {
    esp_netif_t* netif = esp_netif_get_default_netif();
    if (netif == nullptr || !esp_netif_is_netif_up(netif)) {
        return false;
    }
    esp_netif_ip_info_t ip{};
    if (esp_netif_get_ip_info(netif, &ip) != ESP_OK) {
        return false;
    }
    return ip.ip.addr != 0;
}

void RefreshC5UpdateButtonState() {
    if (s_ui.update_btn == nullptr) {
        return;
    }
    const bool can_update = s_ui.version_ready && !s_ui.up_to_date && !s_c5_ota_busy.load() &&
                            IsNetworkReadyForDownload();
    if (can_update) {
        lv_obj_clear_state(s_ui.update_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(s_ui.update_btn, LV_STATE_DISABLED);
    }
}

void ApplyTargetVersionColor(bool same_as_current) {
    if (s_ui.target_ver_label == nullptr) {
        return;
    }
    // 版本一致：与当前版本同色；不一致：高亮目标版本号
    const uint32_t color = same_as_current ? kColorText : kColorAccent;
    lv_obj_set_style_text_color(s_ui.target_ver_label, lv_color_hex(color), LV_PART_MAIN);
}

C5FwVersion ReadC5FwVersion() {
    C5FwVersion out;
    if (esp_hosted_connect_to_slave() != ESP_OK) {
        ESP_LOGW(TAG, "C5 OTA: connect_to_slave failed");
        return out;
    }
    esp_hosted_coprocessor_fwver_t ver{};
    // Prefer RPC directly: public getter may false-fail on transport_up flag.
    if (rpc_get_coprocessor_fwversion(&ver) != ESP_OK) {
        if (esp_hosted_get_coprocessor_fwversion(&ver) != ESP_OK) {
            ESP_LOGW(TAG, "C5 OTA: get fwversion failed");
            return out;
        }
    }
    out.ok = true;
    out.major = ver.major1;
    out.minor = ver.minor1;
    out.patch = ver.patch1;
    return out;
}

enum class C5OtaUiKind : int { Progress = 0, Status = 1, Done = 2, Fail = 3 };

struct C5OtaUiMsg {
    C5OtaUiKind kind;
    int progress;
    char text[96];
};

void ApplyC5OtaUiAsync(void* p) {
    auto* msg = static_cast<C5OtaUiMsg*>(p);
    if (msg == nullptr) {
        return;
    }
    if (s_ui.status_label != nullptr && msg->text[0] != '\0' &&
        (msg->kind == C5OtaUiKind::Status || msg->kind == C5OtaUiKind::Done ||
         msg->kind == C5OtaUiKind::Fail || msg->kind == C5OtaUiKind::Progress)) {
        lv_label_set_text(s_ui.status_label, msg->text);
    }
    if (msg->kind == C5OtaUiKind::Progress || msg->kind == C5OtaUiKind::Done) {
        if (s_ui.progress_bar != nullptr) {
            lv_obj_clear_flag(s_ui.progress_bar, LV_OBJ_FLAG_HIDDEN);
            int pct = msg->progress;
            if (pct < 0) {
                pct = 0;
            }
            if (pct > 100) {
                pct = 100;
            }
            lv_bar_set_value(s_ui.progress_bar, pct, LV_ANIM_OFF);
        }
        if (s_ui.progress_label != nullptr) {
            lv_obj_clear_flag(s_ui.progress_label, LV_OBJ_FLAG_HIDDEN);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d%%", msg->progress < 0 ? 0 : msg->progress);
            lv_label_set_text(s_ui.progress_label, buf);
        }
    }
    if (msg->kind == C5OtaUiKind::Fail) {
        s_c5_ota_busy.store(false);
        RefreshC5UpdateButtonState();
    }
    if (msg->kind == C5OtaUiKind::Done) {
        if (s_ui.progress_bar != nullptr) {
            lv_bar_set_value(s_ui.progress_bar, 100, LV_ANIM_OFF);
        }
        if (s_ui.progress_label != nullptr) {
            lv_label_set_text(s_ui.progress_label, "100%");
        }
    }
    delete msg;
}

void PostC5OtaUi(C5OtaUiKind kind, int progress, const char* text) {
    auto* msg = new C5OtaUiMsg();
    msg->kind = kind;
    msg->progress = progress;
    msg->text[0] = '\0';
    if (text != nullptr) {
        std::snprintf(msg->text, sizeof(msg->text), "%s", text);
    }
    if (lv_async_call(ApplyC5OtaUiAsync, msg) != LV_RESULT_OK) {
        delete msg;
    }
}

void ApplyC5VersionResult(uint32_t gen, const C5FwVersion& ver) {
    s_version_fetching.store(false);
    if (gen != s_ui_gen.load() || s_ui.version_label == nullptr) {
        return;
    }

    char ver_buf[64];
    if (ver.ok) {
        std::snprintf(ver_buf, sizeof(ver_buf), I18n::T("当前版本：%u.%u.%u"),
                      static_cast<unsigned>(ver.major), static_cast<unsigned>(ver.minor),
                      static_cast<unsigned>(ver.patch));
    } else {
        std::snprintf(ver_buf, sizeof(ver_buf), "%s%s", I18n::T("当前版本："), I18n::T("未知"));
    }
    lv_label_set_text(s_ui.version_label, ver_buf);

    s_ui.up_to_date = IsC5TargetVersion(ver);
    s_ui.version_ready = true;
    ApplyTargetVersionColor(s_ui.up_to_date);

    if (s_ui.status_label != nullptr && !s_c5_ota_busy.load()) {
        lv_obj_clear_flag(s_ui.status_label, LV_OBJ_FLAG_HIDDEN);
        if (!ver.ok) {
            lv_label_set_text(s_ui.status_label, I18n::T("无法获取版本"));
        } else if (s_ui.up_to_date) {
            lv_label_set_text(s_ui.status_label, I18n::T("已是最新版本"));
        } else if (!IsNetworkReadyForDownload()) {
            lv_label_set_text(s_ui.status_label, I18n::T("请先连接网络"));
        } else {
            lv_label_set_text(s_ui.status_label, I18n::T("可更新"));
        }
    }

    RefreshC5UpdateButtonState();
}

struct C5VersionUiMsg {
    uint32_t gen = 0;
    C5FwVersion ver;
};

void ApplyC5VersionUiAsync(void* p) {
    auto* msg = static_cast<C5VersionUiMsg*>(p);
    if (msg == nullptr) {
        return;
    }
    ApplyC5VersionResult(msg->gen, msg->ver);
    delete msg;
}

void C5VersionWorkerTask(void* arg) {
    const uint32_t gen = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg));
    ESP_LOGI(TAG, "C5 version: worker start gen=%" PRIu32, gen);
    const C5FwVersion ver = ReadC5FwVersion();
    ESP_LOGI(TAG, "C5 version: worker done ok=%d %u.%u.%u", ver.ok ? 1 : 0,
             static_cast<unsigned>(ver.major), static_cast<unsigned>(ver.minor),
             static_cast<unsigned>(ver.patch));
    auto* msg = new C5VersionUiMsg();
    msg->gen = gen;
    msg->ver = ver;
    if (lv_async_call(ApplyC5VersionUiAsync, msg) != LV_RESULT_OK) {
        s_version_fetching.store(false);
        delete msg;
    }
    vTaskDelete(nullptr);
}

void StartC5VersionFetchIfNeeded() {
    if (s_ui.version_ready || s_c5_ota_busy.load()) {
        return;
    }
    if (s_version_fetching.exchange(true)) {
        return;
    }
    // 先保证按钮灰、提示读取中；真正 RPC 在后台 task，不堵 LVGL/切 Tab
    if (s_ui.update_btn != nullptr) {
        lv_obj_add_state(s_ui.update_btn, LV_STATE_DISABLED);
    }
    if (s_ui.status_label != nullptr) {
        lv_label_set_text(s_ui.status_label, I18n::T("正在读取版本…"));
        lv_obj_clear_flag(s_ui.status_label, LV_OBJ_FLAG_HIDDEN);
    }
    const uint32_t gen = s_ui_gen.load();
    BaseType_t ok =
        xTaskCreate(C5VersionWorkerTask, "c5_ver", 8192,
                    reinterpret_cast<void*>(static_cast<uintptr_t>(gen)), 5, nullptr);
    if (ok != pdPASS) {
        s_version_fetching.store(false);
        if (s_ui.status_label != nullptr) {
            lv_label_set_text(s_ui.status_label, I18n::T("无法获取版本"));
            lv_obj_clear_flag(s_ui.status_label, LV_OBJ_FLAG_HIDDEN);
        }
        ESP_LOGE(TAG, "C5 version: xTaskCreate failed");
    }
}

void RequestC5VersionFetchAsync(void* /*arg*/) {
    StartC5VersionFetchIfNeeded();
}

bool IsC5TabActive(lv_obj_t* tv) {
    if (tv == nullptr || s_ui.tab_index < 0) {
        return false;
    }
    return static_cast<int32_t>(lv_tabview_get_tab_active(tv)) == s_ui.tab_index;
}

void OnC5TabviewChanged(lv_event_t* /*e*/) {
    if (s_ui.tabview == nullptr || !IsC5TabActive(s_ui.tabview)) {
        return;
    }
    // 延后到下一帧：先让 Tab 切过去再开 worker，避免点 Tab 时卡住
    lv_async_call(RequestC5VersionFetchAsync, nullptr);
}

void OnC5TabBtnClicked(lv_event_t* /*e*/) {
    // 同样延后，不在 CLICKED 回调里做任何耗时/RPC
    lv_async_call(RequestC5VersionFetchAsync, nullptr);
}

void C5OtaWorkerTask(void* /*arg*/) {
    auto fail = [](const char* msg) {
        PostC5OtaUi(C5OtaUiKind::Fail, 0, msg);
        vTaskDelete(nullptr);
    };

    PostC5OtaUi(C5OtaUiKind::Status, 0, I18n::T("正在下载并写入…"));
    PostC5OtaUi(C5OtaUiKind::Progress, 0, nullptr);

    auto* network = Board::GetInstance().GetNetwork();
    if (network == nullptr || !IsNetworkReadyForDownload()) {
        fail(I18n::T("请先连接网络"));
        return;
    }
    auto http = network->CreateHttp(0);
    if (!http || !http->Open("GET", kC5FwUrl)) {
        ESP_LOGE(TAG, "C5 OTA: HTTP open failed");
        fail(I18n::T("更新失败"));
        return;
    }
    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "C5 OTA: HTTP status %d", http->GetStatusCode());
        http->Close();
        fail(I18n::T("更新失败"));
        return;
    }
    const size_t content_length = http->GetBodyLength();
    if (content_length == 0) {
        ESP_LOGE(TAG, "C5 OTA: empty body");
        http->Close();
        fail(I18n::T("更新失败"));
        return;
    }

    if (esp_hosted_connect_to_slave() != ESP_OK) {
        http->Close();
        fail(I18n::T("无法获取版本"));
        return;
    }

    esp_err_t err = esp_hosted_slave_ota_begin();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "C5 OTA: begin failed: %s", esp_err_to_name(err));
        http->Close();
        fail(I18n::T("更新失败"));
        return;
    }

    uint8_t chunk[kC5OtaChunkSize];
    size_t total_read = 0;
    int last_pct = -1;
    bool ota_failed = false;

    while (true) {
        const int n = http->Read(reinterpret_cast<char*>(chunk), sizeof(chunk));
        if (n < 0) {
            ESP_LOGE(TAG, "C5 OTA: HTTP read error");
            ota_failed = true;
            break;
        }
        if (n == 0) {
            break;
        }
        err = esp_hosted_slave_ota_write(chunk, static_cast<uint32_t>(n));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "C5 OTA: write failed: %s", esp_err_to_name(err));
            ota_failed = true;
            break;
        }
        total_read += static_cast<size_t>(n);
        int pct = static_cast<int>(total_read * 100 / content_length);
        if (pct > 99) {
            pct = 99;
        }
        if (pct != last_pct) {
            last_pct = pct;
            PostC5OtaUi(C5OtaUiKind::Progress, pct, nullptr);
        }
    }
    http->Close();

    if (ota_failed || total_read == 0) {
        esp_hosted_slave_ota_end();
        fail(I18n::T("更新失败"));
        return;
    }

    err = esp_hosted_slave_ota_end();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "C5 OTA: end failed: %s", esp_err_to_name(err));
        fail(I18n::T("更新失败"));
        return;
    }

    PostC5OtaUi(C5OtaUiKind::Progress, 99, I18n::T("正在激活新固件…"));
    err = esp_hosted_slave_ota_activate();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "C5 OTA: activate failed: %s", esp_err_to_name(err));
        fail(I18n::T("更新失败"));
        return;
    }

    PostC5OtaUi(C5OtaUiKind::Done, 100, I18n::T("更新成功，即将重启"));
    vTaskDelay(pdMS_TO_TICKS(1500));
    // 走 App 重启：先关背光再 esp_restart，避免过渡花屏
    Application::GetInstance().Reboot();
}

void OnC5UpdateClicked(lv_event_t* /*e*/) {
    if (!s_ui.version_ready || s_ui.up_to_date) {
        return;
    }
    if (!IsNetworkReadyForDownload()) {
        if (s_ui.status_label != nullptr) {
            lv_obj_clear_flag(s_ui.status_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(s_ui.status_label, I18n::T("请先连接网络"));
        }
        RefreshC5UpdateButtonState();
        return;
    }
    if (s_c5_ota_busy.exchange(true)) {
        return;
    }
    if (s_ui.update_btn != nullptr) {
        lv_obj_add_state(s_ui.update_btn, LV_STATE_DISABLED);
    }
    if (s_ui.progress_bar != nullptr) {
        lv_obj_clear_flag(s_ui.progress_bar, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(s_ui.progress_bar, 0, LV_ANIM_OFF);
    }
    if (s_ui.progress_label != nullptr) {
        lv_obj_clear_flag(s_ui.progress_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_ui.progress_label, "0%");
    }
    if (s_ui.status_label != nullptr) {
        lv_label_set_text(s_ui.status_label, I18n::T("正在下载并写入…"));
    }
    BaseType_t ok = xTaskCreate(C5OtaWorkerTask, "c5_ota", 12288, nullptr, 5, nullptr);
    if (ok != pdPASS) {
        s_c5_ota_busy.store(false);
        RefreshC5UpdateButtonState();
        if (s_ui.status_label != nullptr) {
            lv_label_set_text(s_ui.status_label, I18n::T("更新失败"));
        }
        ESP_LOGE(TAG, "C5 OTA: xTaskCreate failed");
    }
}

}  // namespace

void SettingsUpgradeTab_Reset() {
    s_ui_gen.fetch_add(1);
    if (s_ui.tabview != nullptr) {
        lv_obj_remove_event_cb(s_ui.tabview, OnC5TabviewChanged);
    }
    if (s_ui.tab_btn != nullptr) {
        lv_obj_remove_event_cb(s_ui.tab_btn, OnC5TabBtnClicked);
    }
    s_ui = {};
    s_version_fetching.store(false);
}

void SettingsUpgradeTab_Build(lv_obj_t* tab) {
    s_ui_gen.fetch_add(1);
    s_ui = {};
    s_ui.tab = tab;
    s_ui.version_ready = false;
    s_ui.up_to_date = false;

    lv_obj_set_style_pad_all(tab, 24, LV_PART_MAIN);
    lv_obj_set_style_pad_row(tab, 16, LV_PART_MAIN);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hint = lv_label_create(tab);
    lv_label_set_text(hint, I18n::T("C5固件"));
    lv_obj_set_style_text_color(hint, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &font_puhui_20_4, LV_PART_MAIN);

    char ver_placeholder[64];
    std::snprintf(ver_placeholder, sizeof(ver_placeholder), "%s…", I18n::T("当前版本："));

    s_ui.version_label = lv_label_create(tab);
    lv_label_set_text(s_ui.version_label, ver_placeholder);
    lv_obj_set_style_text_color(s_ui.version_label, lv_color_hex(kColorText), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_ui.version_label, &font_puhui_30_4, LV_PART_MAIN);

    lv_obj_t* target_row = lv_obj_create(tab);
    screen_strip_obj_chrome(target_row);
    lv_obj_set_width(target_row, LV_PCT(100));
    lv_obj_set_height(target_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(target_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_flex_flow(target_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(target_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(target_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(target_row, 0, LV_PART_MAIN);
    lv_obj_remove_flag(target_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(target_row, LV_OBJ_FLAG_CLICKABLE);

    s_ui.target_prefix_label = lv_label_create(target_row);
    lv_label_set_text(s_ui.target_prefix_label, I18n::T("目标版本："));
    lv_obj_set_style_text_color(s_ui.target_prefix_label, lv_color_hex(kColorText),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(s_ui.target_prefix_label, &font_puhui_30_4, LV_PART_MAIN);

    s_ui.target_ver_label = lv_label_create(target_row);
    lv_label_set_text(s_ui.target_ver_label, "2.12.12");
    lv_obj_set_style_text_font(s_ui.target_ver_label, &font_puhui_30_4, LV_PART_MAIN);
    // 读版本前先与当前版本同色；结果回来后再按是否一致改色
    ApplyTargetVersionColor(true);

    s_ui.status_label = lv_label_create(tab);
    lv_label_set_text(s_ui.status_label, I18n::T("正在读取版本…"));
    lv_obj_set_style_text_color(s_ui.status_label, lv_color_hex(kColorValue), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_ui.status_label, &font_puhui_20_4, LV_PART_MAIN);

    s_ui.progress_bar = lv_bar_create(tab);
    lv_obj_set_width(s_ui.progress_bar, LV_PCT(100));
    lv_obj_set_height(s_ui.progress_bar, 18);
    lv_bar_set_range(s_ui.progress_bar, 0, 100);
    lv_bar_set_value(s_ui.progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_ui.progress_bar, lv_color_hex(kColorSliderTrack), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ui.progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_ui.progress_bar, 9, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.progress_bar, lv_color_hex(kColorAccent), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_ui.progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_ui.progress_bar, 9, LV_PART_INDICATOR);
    lv_obj_add_flag(s_ui.progress_bar, LV_OBJ_FLAG_HIDDEN);

    s_ui.progress_label = lv_label_create(tab);
    lv_label_set_text(s_ui.progress_label, "0%");
    lv_obj_set_style_text_color(s_ui.progress_label, lv_color_hex(kColorText), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_ui.progress_label, &font_puhui_20_4, LV_PART_MAIN);
    lv_obj_add_flag(s_ui.progress_label, LV_OBJ_FLAG_HIDDEN);

    s_ui.update_btn = lv_button_create(tab);
    lv_obj_set_width(s_ui.update_btn, LV_PCT(100));
    lv_obj_set_height(s_ui.update_btn, 64);
    lv_obj_set_style_bg_color(s_ui.update_btn, lv_color_hex(kColorAccent), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ui.update_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_ui.update_btn, 16, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.update_btn, lv_color_hex(0x4B5563),
                              LV_PART_MAIN | LV_STATE_DISABLED);
    screen_swipe_back_ignore(s_ui.update_btn, true);
    lv_obj_add_event_cb(s_ui.update_btn, OnC5UpdateClicked, LV_EVENT_CLICKED, nullptr);
    // 读完版本前不可点
    lv_obj_add_state(s_ui.update_btn, LV_STATE_DISABLED);

    lv_obj_t* btn_lbl = lv_label_create(s_ui.update_btn);
    lv_label_set_text(btn_lbl, I18n::T("更新"));
    lv_obj_set_style_text_color(btn_lbl, lv_color_hex(kColorText), LV_PART_MAIN);
    lv_obj_set_style_text_font(btn_lbl, &font_puhui_30_4, LV_PART_MAIN);
    lv_obj_center(btn_lbl);

    lv_obj_t* foot = lv_label_create(tab);
    lv_label_set_text(foot, I18n::T("更新完成后设备会自动重启"));
    lv_obj_set_style_text_color(foot, lv_color_hex(kColorSubtle), LV_PART_MAIN);
    lv_obj_set_style_text_font(foot, &font_puhui_20_4, LV_PART_MAIN);

    // tab -> content -> tabview；点到本 Tab 后再异步读版本
    lv_obj_t* content = lv_obj_get_parent(tab);
    lv_obj_t* tv = content != nullptr ? lv_obj_get_parent(content) : nullptr;
    s_ui.tabview = tv;
    s_ui.tab_index = -1;
    if (content != nullptr) {
        const uint32_t n = lv_obj_get_child_count(content);
        for (uint32_t i = 0; i < n; ++i) {
            if (lv_obj_get_child(content, i) == tab) {
                s_ui.tab_index = static_cast<int32_t>(i);
                break;
            }
        }
    }
    if (tv != nullptr && s_ui.tab_index >= 0) {
        lv_obj_add_event_cb(tv, OnC5TabviewChanged, LV_EVENT_VALUE_CHANGED, nullptr);
        lv_obj_t* bar = lv_tabview_get_tab_bar(tv);
        if (bar != nullptr) {
            s_ui.tab_btn = lv_obj_get_child_by_type(bar, static_cast<int32_t>(s_ui.tab_index),
                                                    &lv_button_class);
            if (s_ui.tab_btn != nullptr) {
                lv_obj_add_event_cb(s_ui.tab_btn, OnC5TabBtnClicked, LV_EVENT_CLICKED, nullptr);
            }
        }
        if (IsC5TabActive(tv)) {
            lv_async_call(RequestC5VersionFetchAsync, nullptr);
        }
    } else {
        ESP_LOGW(TAG, "C5 version: tabview/index unresolved, fetch on demand unavailable");
    }
}

#endif  // CONFIG_ESP_HOSTED_ENABLED
