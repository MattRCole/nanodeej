#pragma once
#include "led_api.h"
#include "hmi_api.h"
#include <map>
#include "HapticProfileManager.h"

#define APP_DEV_COLOR_NOT_DEFINED -1
#define APP_DEV_IS_UNDEFINED(color) ((color) < 1)
#define APP_DEV_WITH_DEFAULT(config_obj, config_name) ((uint32_t)(config_obj.config_name > -1 ? config_obj.config_name : NanoProfiles::defaultColorConfig.config_name))

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
        int8_t mappedKey;
        int32_t keyColor;
        int32_t ringPrimary;
        int32_t ringSecondary;
        int32_t ringPointer;
    } devAppInfo;

    typedef struct
    {
        int32_t keyColor;
        int32_t ringPrimary;
        int32_t ringSecondary;
        int32_t ringPointer;
    } keyColorConfig;


    extern keyColorConfig defaultColorConfig;
    extern devAppInfo apps[];
    extern std::map<String, devAppInfo *> app_map;
};
