#include "app_usb.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_private/usb_phy.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "usb_descriptors.h"

#if __has_include("config.h")
#include "config.h"
#endif
#ifndef USB_OTG_DM_PIN
#define USB_OTG_DM_PIN 24
#endif
#ifndef USB_OTG_DP_PIN
#define USB_OTG_DP_PIN 25
#endif

#include "driver/gpio.h"
#include "hal/usb_serial_jtag_ll.h"
#include "hal/usb_wrap_ll.h"
#include "soc/usb_wrap_struct.h"

static const char* TAG = "app_usb";
static usb_phy_handle_t s_phy_hdl = NULL;
static bool s_driver_installed = false;

static void RoutePhyToOtg(void) {
    usb_serial_jtag_ll_phy_enable_pad(false);
    usb_wrap_ll_phy_select(&USB_WRAP, 0);
    gpio_set_drive_capability((gpio_num_t)USB_OTG_DM_PIN, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability((gpio_num_t)USB_OTG_DP_PIN, GPIO_DRIVE_CAP_3);
}

static void RoutePhyToUsj(void) {
    usb_wrap_ll_phy_select(&USB_WRAP, 1);
    usb_serial_jtag_ll_phy_set_defaults();
    usb_serial_jtag_ll_phy_enable_pad(true);
}

static esp_err_t usb_phy_init_fs(void) {
    if (s_phy_hdl) {
        return ESP_OK;
    }
    RoutePhyToOtg();
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .otg_speed = USB_PHY_SPEED_FULL,
    };
    ESP_RETURN_ON_ERROR(usb_new_phy(&phy_conf, &s_phy_hdl), TAG, "usb_new_phy");
    gpio_set_drive_capability((gpio_num_t)USB_OTG_DM_PIN, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability((gpio_num_t)USB_OTG_DP_PIN, GPIO_DRIVE_CAP_3);
    return ESP_OK;
}

static void usb_stack_teardown(void) {
#if CFG_TUD_VENDOR
    app_vendor_deinit();
#endif
#if CFG_TUD_HID
    app_touch_deinit();
    app_hid_deinit();
#endif
    if (s_driver_installed) {
        tinyusb_driver_uninstall();
        s_driver_installed = false;
    }
    if (s_phy_hdl != NULL) {
        usb_del_phy(s_phy_hdl);
        s_phy_hdl = NULL;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    RoutePhyToUsj();
}

esp_err_t app_usb_init(void) {
    esp_err_t err = usb_phy_init_fs();
    if (err != ESP_OK) {
        return err;
    }

    tinyusb_config_t tusb_cfg = TINYUSB_CONFIG_FULL_SPEED(NULL, NULL);
    tusb_cfg.phy.skip_setup = true;
    tusb_cfg.task =
        TINYUSB_TASK_CUSTOM(4096, CONFIG_USB_TASK_PRIORITY, 0);
    tusb_cfg.descriptor.device = usb_extend_get_device_desc();
    tusb_cfg.descriptor.full_speed_config = usb_extend_get_fs_config_desc();
    tusb_cfg.descriptor.string = usb_extend_get_string_desc();
    tusb_cfg.descriptor.string_count =
        (size_t)usb_extend_get_string_count();

    err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_driver_install: %s", esp_err_to_name(err));
        usb_stack_teardown();
        return err;
    }
    s_driver_installed = true;

#if CFG_TUD_VENDOR
    err = app_vendor_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "vendor: %s", esp_err_to_name(err));
        usb_stack_teardown();
        return err;
    }
#endif
#if CFG_TUD_HID
    err = app_hid_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "hid: %s", esp_err_to_name(err));
        usb_stack_teardown();
        return err;
    }
    err = app_touch_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "touch: %s", esp_err_to_name(err));
        usb_stack_teardown();
        return err;
    }
#endif
#if CONFIG_UAC_AUDIO_ENABLE
    err = app_uac_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uac: %s", esp_err_to_name(err));
        usb_stack_teardown();
        return err;
    }
#endif

    ESP_LOGI(TAG, "USB extend gadget up (FS VID=0x%04X PID=0x%04X audio=%d)",
             (unsigned)USB_VID, (unsigned)USB_PID, CONFIG_UAC_AUDIO_ENABLE);
    return ESP_OK;
}

esp_err_t app_usb_deinit(void) {
    usb_stack_teardown();
    ESP_LOGI(TAG, "USB extend gadget down, USJ restored");
    return ESP_OK;
}
