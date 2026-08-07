#include "app_usb.h"

#if CONFIG_UAC_AUDIO_ENABLE

#include <cstring>
#include <vector>

#include "audio_codec.h"
#include "board.h"
#include "esp_check.h"
#include "esp_log.h"
#include "usb_device_uac.h"
#include "usb_descriptors.h"

static const char* TAG = "app_uac";

static esp_err_t uac_device_output_cb(uint8_t* buf, size_t len, void* arg) {
    (void)arg;
    if (buf == nullptr || len < 2) {
        return ESP_OK;
    }
    auto* codec = Board::GetInstance().GetAudioCodec();
    if (codec == nullptr) {
        return ESP_OK;
    }
    if (!codec->output_enabled()) {
        codec->EnableOutput(true);
    }

    // UAC 16-bit PCM：立体声则取左声道写给板级 codec（内部会复制到双声道）。
    const size_t sample_bytes = sizeof(int16_t);
#if CONFIG_UAC_SPEAKER_CHANNEL_NUM >= 2
    const size_t frame_samples = 2;
#else
    const size_t frame_samples = 1;
#endif
    const size_t frames = len / (sample_bytes * frame_samples);
    if (frames == 0) {
        return ESP_OK;
    }

    std::vector<int16_t> mono(frames);
    const auto* in = reinterpret_cast<const int16_t*>(buf);
    for (size_t i = 0; i < frames; ++i) {
        mono[i] = in[i * frame_samples];
    }
    codec->OutputData(mono);
    return ESP_OK;
}

#if CONFIG_UAC_MIC_CHANNEL_NUM > 0
static esp_err_t uac_device_input_cb(uint8_t* buf, size_t len, size_t* bytes_read,
                                     void* arg) {
    (void)arg;
    if (buf == nullptr || bytes_read == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(buf, 0, len);
    *bytes_read = len;
    return ESP_OK;
}
#endif

static void uac_device_set_mute_cb(uint32_t mute, void* arg) {
    (void)arg;
    auto* codec = Board::GetInstance().GetAudioCodec();
    if (codec == nullptr) {
        return;
    }
    if (mute) {
        codec->EnableOutput(false);
    } else {
        codec->EnableOutput(true);
    }
    ESP_LOGD(TAG, "mute=%" PRIu32, mute);
}

static void uac_device_set_volume_cb(uint32_t volume, void* arg) {
    (void)arg;
    auto* codec = Board::GetInstance().GetAudioCodec();
    if (codec == nullptr) {
        return;
    }
    // UAC volume 0..100
    if (volume > 100) {
        volume = 100;
    }
    codec->SetOutputVolume(static_cast<int>(volume));
}

extern "C" esp_err_t app_uac_init(void) {
    auto* codec = Board::GetInstance().GetAudioCodec();
    ESP_RETURN_ON_FALSE(codec != nullptr, ESP_ERR_INVALID_STATE, TAG, "no codec");
    codec->EnableOutput(true);

    uac_device_config_t config = {};
    config.skip_tinyusb_init = true;
    config.output_cb = uac_device_output_cb;
#if CONFIG_UAC_MIC_CHANNEL_NUM > 0
    config.input_cb = uac_device_input_cb;
#else
    config.input_cb = nullptr;
#endif
    config.set_mute_cb = uac_device_set_mute_cb;
    config.set_volume_cb = uac_device_set_volume_cb;
    config.cb_ctx = nullptr;
#if CONFIG_USB_DEVICE_UAC_AS_PART
#if CONFIG_UAC_SPEAKER_CHANNEL_NUM > 0
    config.spk_itf_num = ITF_NUM_AUDIO_STREAMING_SPK;
#endif
#if CONFIG_UAC_MIC_CHANNEL_NUM > 0
    config.mic_itf_num = ITF_NUM_AUDIO_STREAMING_MIC;
#endif
#endif
    ESP_RETURN_ON_ERROR(uac_device_init(&config), TAG, "uac_device_init");
    ESP_LOGI(TAG, "UAC speaker-only ready rate=%d ch=%d", CONFIG_UAC_SAMPLE_RATE,
             CONFIG_UAC_SPEAKER_CHANNEL_NUM);
    return ESP_OK;
}

#endif  // CONFIG_UAC_AUDIO_ENABLE
