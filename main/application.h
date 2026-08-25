#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_timer.h>

#include <string>
#include <mutex>
#include <deque>
#include <memory>

#include "protocol.h"
#include "ota.h"
#include "audio_service.h"
#include "device_state_event.h"


#define MAIN_EVENT_SCHEDULE (1 << 0)
#define MAIN_EVENT_SEND_AUDIO (1 << 1)
#define MAIN_EVENT_WAKE_WORD_DETECTED (1 << 2)
#define MAIN_EVENT_VAD_CHANGE (1 << 3)
#define MAIN_EVENT_ERROR (1 << 4)
#define MAIN_EVENT_CHECK_NEW_VERSION_DONE (1 << 5)
#define MAIN_EVENT_CLOCK_TICK (1 << 6)


enum AecMode {
    kAecOff,
    kAecOnDeviceSide,
};

// 产品策略：打断（设备端 AEC / Realtime 聆听）是否对用户开放。
// false：聊天页不展示开关；运行时强制保持 Off，并清除 NVS 中旧的「开」残留，
//        避免「UI 已隐藏但 NVS 仍恢复打断」的半状态。
// 恢复能力时改为 true，并确保聊天菜单重新挂上打断项。
inline constexpr bool kInterruptUserControlEnabled = false;

class Application {
public:
    static Application& GetInstance() {
        static Application instance;
        return instance;
    }
    // 删除拷贝构造函数和赋值运算符
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Start();
    void MainEventLoop();
    DeviceState GetDeviceState() const { return device_state_; }
    bool IsVoiceDetected() const { return audio_service_.IsVoiceDetected(); }
    void Schedule(std::function<void()> callback);
    void SetDeviceState(DeviceState state);
    void Alert(const char* status, const char* message, const char* emotion = "", const std::string_view& sound = "");
    void DismissAlert();
    void AbortSpeaking(AbortReason reason);
    void ToggleChatState();
    void StartListening();
    void StopListening();
    void Reboot();
    void WakeWordInvoke(const std::string& wake_word);
    bool UpgradeFirmware(Ota& ota, const std::string& url = "");
    bool CanEnterSleepMode();
    void SendMcpMessage(const std::string& payload);
    void SetAecMode(AecMode mode);
    AecMode GetAecMode() const { return aec_mode_; }
    void PlaySound(const std::string_view& sound);
    AudioService& GetAudioService() { return audio_service_; }

    // 语音 UI 会话（聊天页 / 数字人页）：唤醒词仅在会话内开启。
    // desired=false：立刻软停（停 Feed / disable_wakenet），延迟硬 destroy AFE，
    // 以便快速再进聊天/数字人时复用引擎，避免低内存下重建崩溃。
    void SetVoiceUiDesired(bool desired);
    bool IsVoiceUiActive() const { return voice_ui_active_; }
    bool IsVoiceUiDesired() const { return voice_ui_desired_; }

    bool HasPendingActivation() const {
        return !activation_suspended_ && !pending_activation_code_.empty();
    }
    const std::string& GetPendingActivationCode() const { return pending_activation_code_; }
    // 启动流水线已走到 Idle，且当前无需等待激活码 / 不在 activating。
    bool IsDeviceActivated() const;
    bool IsBootReady() const { return boot_ready_; }
    void SetActivationSuspended(bool suspended);
    bool IsActivationSuspended() const { return activation_suspended_; }
    void StopSystemAudioForStressTest();
    void RestoreSystemAudioAfterStressTest();

private:
    Application();
    ~Application();

    std::mutex mutex_;
    std::deque<std::function<void()>> main_tasks_;
    std::unique_ptr<Protocol> protocol_;
    EventGroupHandle_t event_group_ = nullptr;
    esp_timer_handle_t clock_timer_handle_ = nullptr;
    esp_timer_handle_t voice_ui_release_timer_ = nullptr;
    esp_timer_handle_t voice_ui_start_retry_timer_ = nullptr;
    volatile DeviceState device_state_ = kDeviceStateUnknown;
    ListeningMode listening_mode_ = kListeningModeAutoStop;
    AecMode aec_mode_ = kAecOff;
    std::string last_error_message_;
    AudioService audio_service_;
    std::string pending_activation_code_;
    volatile bool activation_suspended_ = false;
    // 仅在 Application::Start() 末尾首次进入 Idle 后置位；starting/activating 期间为 false。
    volatile bool boot_ready_ = false;
    // UI 期望：页面 enter/leave 写入；Sync 在主循环对齐实际会话。
    volatile bool voice_ui_desired_ = false;
    // 实际会话：true 时 Idle 才允许开唤醒词。
    volatile bool voice_ui_active_ = false;
    // 使延迟 Release / 启动重试失效（leave/enter 递增）。
    volatile uint32_t voice_ui_epoch_ = 0;
    uint32_t voice_ui_pending_release_epoch_ = 0;
    uint32_t voice_ui_pending_retry_epoch_ = 0;

    void SyncVoiceUiSession();
    void TearDownVoiceAudioPaths(bool release_wake_word);
    void SoftStopVoiceAudioPaths();
    void ParkVoiceUiProtocol();
    void ApplyVoiceUiStart();
    void ApplyVoiceUiStop();
    void ScheduleVoiceUiHardRelease(uint32_t epoch);
    void CancelVoiceUiHardRelease();
    void ScheduleVoiceUiStartRetry(uint32_t epoch);
    bool TryEnableWakeWordForVoiceUi();

    bool has_server_time_ = false;
    bool aborted_ = false;
    int clock_ticks_ = 0;
    TaskHandle_t check_new_version_task_handle_ = nullptr;
    TaskHandle_t main_event_loop_task_handle_ = nullptr;

    void OnWakeWordDetected();
    void CheckNewVersion(Ota& ota);
    void CheckAssetsVersion();
    void ShowActivationCode(const std::string& code, const std::string& message);
    void SetListeningMode(ListeningMode mode);
    // 从 NVS 恢复「打断」偏好到 aec_mode_，并同步到音频处理器（开机静默，不弹通知）。
    void ApplyInterruptPreferenceFromNvs();
};


class TaskPriorityReset {
public:
    TaskPriorityReset(BaseType_t priority) {
        original_priority_ = uxTaskPriorityGet(NULL);
        vTaskPrioritySet(NULL, priority);
    }
    ~TaskPriorityReset() {
        vTaskPrioritySet(NULL, original_priority_);
    }

private:
    BaseType_t original_priority_;
};

#endif // _APPLICATION_H_
