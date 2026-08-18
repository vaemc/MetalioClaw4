#include "settings_bluetooth_tab.h"

#include "bluetooth_screen/bluetooth_screen.h"

void SettingsBluetoothTab_Build(lv_obj_t* tab) {
    lv_obj_set_style_bg_opa(tab, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tab, 0, LV_PART_MAIN);
    lv_obj_add_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    BluetoothScreen::BuildInto(tab);
}
