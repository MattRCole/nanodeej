#pragma once
#include "led_api.h"
#include "hmi_api.h"
#include "HapticProfileManager.h"

namespace NanoProfiles
{

    extern ledConfig default_led_config;
    extern knobValue default_knob_value;
    extern knobMapping default_knob_mapping;
    extern hmiConfig default_hmi_config;

    extern HapticProfile default_haptic_profile;
    typedef struct
    {
        String type;
        String id;
        String title;
        uint16_t volume;
        uint16_t volumeMax;
    } devAppInfo;

    extern devAppInfo apps[];
};
