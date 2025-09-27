
#include "./HapticProfileManager.h"


void HapticProfileManager::updateProfile(HapticProfile* profile, uint8_t from_version) {
    if (from_version==1) {
        // Reset all key actions to none since we no longer support MIDI/HID
        profile->hmi_config.keys[0].num_pressed_actions = 0;
        profile->hmi_config.keys[1].num_pressed_actions = 0;
        profile->hmi_config.keys[2].num_pressed_actions = 0;
        profile->hmi_config.keys[3].num_pressed_actions = 0;
        Serial.println("{\"type\":\"debug\",\"msg\":\"Updated profile " + profile->profile_name + " from version 1 to 2 (removed MIDI/HID actions)\"}");
    }
}