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
    // 页面侧请用 Schedule*，勿在 LVGL worker 里同步跑重逻辑。
    // Stop 带 epoch：聊天↔数字人切换时，后到的 Start 会使旧 Stop 失效，避免会话被误关。
    void StartVoiceUiSession();
    void StopVoiceUiSession(uint32_t epoch);
    void ScheduleStartVoiceUiSession();
    void ScheduleStopVoiceUiSession();
    bool IsVoiceUiActive() const { return voice_ui_active_; }
    uint32_t VoiceUiEpoch() const { return voice_ui_epoch_; }

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
    volatile DeviceState device_state_ = kDeviceStateUnknown;
    ListeningMode listening_mode_ = kListeningModeAutoStop;
    AecMode aec_mode_ = kAecOff;
    std::string last_error_message_;
    AudioService audio_service_;
    std::string pending_activation_code_;
    volatile bool activation_suspended_ = false;
    // 仅在 Application::Start() 末尾首次进入 Idle 后置位；starting/activating 期间为 false。
    volatile bool boot_ready_ = false;
    // 聊天/数字人页持有的对话会话；false 时 Idle 不得开唤醒词。
    volatile bool voice_ui_active_ = false;
    // Start 递增；ScheduleStop 捕获当时的 epoch，过期则跳过。
    uint32_t voice_ui_epoch_ = 0;

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
