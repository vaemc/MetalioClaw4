#include "usb_extend_prefs.h"

#include "settings.h"
#include "usb_extend_defaults.h"

namespace {

constexpr const char* kNs = "ui";
constexpr const char* kKeyJpeg = "ues_jpg";
constexpr const char* kKeyFps = "ues_fps";
constexpr const char* kKeyFrame = "ues_frm";

int Clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int s_jpeg = USB_EXTEND_DEFAULT_JPEG_QUALITY;
int s_fps = USB_EXTEND_DEFAULT_MAX_FPS;
int s_frame = USB_EXTEND_DEFAULT_FRAME_LIMIT_B;
bool s_loaded = false;

}  // namespace

extern "C" void usb_extend_prefs_load(void) {
    Settings s(kNs, false);
    s_jpeg = Clamp(s.GetInt(kKeyJpeg, USB_EXTEND_DEFAULT_JPEG_QUALITY), 1, 10);
    s_fps = Clamp(s.GetInt(kKeyFps, USB_EXTEND_DEFAULT_MAX_FPS), 1, 60);
    s_frame = Clamp(s.GetInt(kKeyFrame, USB_EXTEND_DEFAULT_FRAME_LIMIT_B), 32 * 1024,
                    300 * 1024);
    s_loaded = true;
}

extern "C" int usb_extend_prefs_get_jpeg_quality(void) {
    if (!s_loaded) {
        usb_extend_prefs_load();
    }
    return s_jpeg;
}

extern "C" int usb_extend_prefs_get_max_fps(void) {
    if (!s_loaded) {
        usb_extend_prefs_load();
    }
    return s_fps;
}

extern "C" int usb_extend_prefs_get_frame_limit_b(void) {
    if (!s_loaded) {
        usb_extend_prefs_load();
    }
    return s_frame;
}

extern "C" void usb_extend_prefs_set_jpeg_quality(int quality) {
    s_jpeg = Clamp(quality, 1, 10);
    Settings s(kNs, true);
    s.SetInt(kKeyJpeg, s_jpeg);
    s_loaded = true;
}

extern "C" void usb_extend_prefs_set_max_fps(int fps) {
    s_fps = Clamp(fps, 1, 60);
    Settings s(kNs, true);
    s.SetInt(kKeyFps, s_fps);
    s_loaded = true;
}

extern "C" void usb_extend_prefs_set_frame_limit_b(int bytes) {
    s_frame = Clamp(bytes, 32 * 1024, 300 * 1024);
    Settings s(kNs, true);
    s.SetInt(kKeyFrame, s_frame);
    s_loaded = true;
}
