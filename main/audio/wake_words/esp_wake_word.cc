#include "esp_wake_word.h"
#include <esp_log.h>


#define TAG "EspWakeWord"

EspWakeWord::EspWakeWord() {
}

EspWakeWord::~EspWakeWord() {
    Deinitialize();
    if (models_owned_ && wakenet_model_ != nullptr) {
        esp_srmodel_deinit(wakenet_model_);
        wakenet_model_ = nullptr;
    }
}

void EspWakeWord::Deinitialize() {
    running_ = false;
    if (wakenet_data_ != nullptr && wakenet_iface_ != nullptr) {
        wakenet_iface_->destroy(wakenet_data_);
        wakenet_data_ = nullptr;
    }
}

bool EspWakeWord::Initialize(AudioCodec* codec, srmodel_list_t* models_list) {
    codec_ = codec;

    if (wakenet_data_ != nullptr) {
        return true;
    }

    if (wakenet_model_ == nullptr) {
        if (models_list == nullptr) {
            wakenet_model_ = esp_srmodel_init("model");
            models_owned_ = true;
        } else {
            wakenet_model_ = models_list;
            models_owned_ = false;
        }
    }

    if (wakenet_model_ == nullptr || wakenet_model_->num == -1) {
        ESP_LOGE(TAG, "Failed to initialize wakenet model");
        return false;
    }
    if (wakenet_model_->num > 1) {
        ESP_LOGW(TAG, "More than one model found, using the first one");
    } else if (wakenet_model_->num == 0) {
        ESP_LOGE(TAG, "No model found");
        return false;
    }
    char* model_name = wakenet_model_->model_name[0];
    wakenet_iface_ = (esp_wn_iface_t*)esp_wn_handle_from_name(model_name);
    wakenet_data_ = wakenet_iface_->create(model_name, DET_MODE_95);

    int frequency = wakenet_iface_->get_samp_rate(wakenet_data_);
    int audio_chunksize = wakenet_iface_->get_samp_chunksize(wakenet_data_);
    ESP_LOGI(TAG, "Wake word(%s),freq: %d, chunksize: %d", model_name, frequency, audio_chunksize);

    return true;
}

void EspWakeWord::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
    wake_word_detected_callback_ = callback;
}

void EspWakeWord::Start() {
    running_ = true;
}

void EspWakeWord::Stop() {
    running_ = false;
}

void EspWakeWord::Feed(const std::vector<int16_t>& data) {
    if (wakenet_data_ == nullptr || !running_) {
        return;
    }

    int res = wakenet_iface_->detect(wakenet_data_, (int16_t*)data.data());
    if (res > 0) {
        last_detected_wake_word_ = wakenet_iface_->get_word_name(wakenet_data_, res);
        running_ = false;

        if (wake_word_detected_callback_) {
            wake_word_detected_callback_(last_detected_wake_word_);
        }
    }
}

size_t EspWakeWord::GetFeedSize() {
    if (wakenet_data_ == nullptr) {
        return 0;
    }
    return wakenet_iface_->get_samp_chunksize(wakenet_data_);
}

void EspWakeWord::EncodeWakeWordData() {
}

bool EspWakeWord::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    return false;
}
