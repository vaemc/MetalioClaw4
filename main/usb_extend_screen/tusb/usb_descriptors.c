#include <stdio.h>
#include <string.h>

#include "tusb.h"
#include "tusb_config.h"
#include "uac_config.h"
#include "uac_descriptors.h"
#include "usb_descriptors.h"
#include "usb_extend_defaults.h"
#include "usb_extend_prefs.h"

#if CFG_TUD_HID
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_TOUCH_SCREEN(REPORT_ID_TOUCH, USB_EXTEND_SCREEN_HEIGHT,
                                     USB_EXTEND_SCREEN_WIDTH),
};

uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return desc_hid_report;
}
#endif

enum {
    STR_INDEX_VENDOR = 4,
#if CFG_TUD_HID
    STR_INDEX_HID,
#endif
#if CONFIG_UAC_AUDIO_ENABLE
    STR_INDEX_AUDIO,
#endif
};

#if CONFIG_UAC_AUDIO_ENABLE
#define CONFIG_AUDIO_DESC_LEN (TUD_AUDIO_DEVICE_DESC_LEN)
#else
#define CONFIG_AUDIO_DESC_LEN 0
#endif

#define CONFIG_TOTAL_LEN                                           \
    (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN * CFG_TUD_HID +         \
     TUD_VENDOR_DESC_LEN * CFG_TUD_VENDOR + CONFIG_AUDIO_DESC_LEN)

uint8_t const desc_fs_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
#if CFG_TUD_VENDOR
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, STR_INDEX_VENDOR, EPNUM_VENDOR,
                          0x80 | EPNUM_VENDOR, CFG_TUD_VENDOR_EPSIZE),
#endif
#if CFG_TUD_HID
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, STR_INDEX_HID, HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_report), (0x80 | EPNUM_HID_DATA),
                       CFG_TUD_HID_EP_BUFSIZE, 10),
#endif
#if CONFIG_UAC_AUDIO_ENABLE
#if CONFIG_UAC_MIC_CHANNEL_NUM > 0
    TUD_AUDIO_DESCRIPTOR(ITF_NUM_AUDIO_CONTROL, STR_INDEX_AUDIO, EPNUM_AUDIO_OUT,
                         (0x80 | EPNUM_AUDIO_IN), (0x80 | EPNUM_AUDIO_FB)),
#else
    /* 仅扬声器：epin 参数被 SPEAK 宏忽略 */
    TUD_AUDIO_DESCRIPTOR(ITF_NUM_AUDIO_CONTROL, STR_INDEX_AUDIO, EPNUM_AUDIO_OUT,
                         0xFF, (0x80 | EPNUM_AUDIO_FB)),
#endif
#endif
};

tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_UNSPECIFIED,
    .bDeviceSubClass = TUSB_CLASS_UNSPECIFIED,
    .bDeviceProtocol = TUSB_CLASS_UNSPECIFIED,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0101,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static char s_vendor_str[96];

static void refresh_vendor_str(void) {
    snprintf(s_vendor_str, sizeof(s_vendor_str),
             "%sudisp0_R%dx%d_Ejpg%d_Fps%d_Bl%d", CONFIG_IDF_TARGET,
             USB_EXTEND_SCREEN_HEIGHT, USB_EXTEND_SCREEN_WIDTH,
             usb_extend_prefs_get_jpeg_quality(), usb_extend_prefs_get_max_fps(),
             usb_extend_prefs_get_frame_limit_b());
}

char const* string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    USB_MANUFACTURER,
    "ESP_Extern_Screen",
    "012-2021",
    s_vendor_str,
#if CFG_TUD_HID
    "touch",
#endif
#if CONFIG_UAC_AUDIO_ENABLE
    "esp uac",
    "speaker",
#if CONFIG_UAC_MIC_CHANNEL_NUM > 0
    "mic",
#endif
#endif
};

const tusb_desc_device_t* usb_extend_get_device_desc(void) {
    return &desc_device;
}

const uint8_t* usb_extend_get_fs_config_desc(void) {
    return desc_fs_configuration;
}

const char** usb_extend_get_string_desc(void) {
    refresh_vendor_str();
    return (const char**)string_desc_arr;
}

int usb_extend_get_string_count(void) {
    return (int)(sizeof(string_desc_arr) / sizeof(string_desc_arr[0]));
}
