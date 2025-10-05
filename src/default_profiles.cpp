#include "default_profiles.h"

namespace NanoProfiles {
    ledConfig default_led_config{};
    knobValue default_knob_value = {
        .key_state=0,
        .type =  knobValueType::KV_ACTIONS,
        .value_min = 0.0,
        .value_max = 127,                        // Maximum value (MIDI range)
        .angle_min = 0,                          // Start angle (0 radians)
        .angle_max = 6.28318f,                   // End angle (2π radians = full rotation)
        .max_exclusive = false,                  // angle_max is inclusive
        .wrap = false,                           // Don't wrap around
        .step = 1,                              // Integer steps
        .recenter = false,                      // Use absolute angles
        .haptic = {
            .mode = HapticMode::REGULAR,
            .start_pos = 0,
            .end_pos =100,
            .detent_count = 100,
            .vernier = 5,
            .kxForce = false
        },
        .actions = {},
    };
    knobMapping default_knob_mapping{};
    hmiConfig default_hmi_config = {
        .keys = {},
        .knob = default_knob_mapping,
    };

    HapticProfile default_haptic_profile{};


    keyColorConfig defaultColorConfig = {
        .keyColor=0x08596C,
        .ringPrimary=0x08596C,
        .ringSecondary=0x47040D,
        .ringPointer=0xFFFFFF,
    };

    devAppInfo apps[4] = {
        {
            .type="application",
            .id="spotify",
            .title="Spotify",
            .volume=0,
            .volumeMax=100,
            .mappedKey=0,
            .keyColor=0x08596C,
            .ringPrimary=0x08596C,
            .ringSecondary=0x47040D,
            .ringPointer=APP_DEV_COLOR_NOT_DEFINED
        },
        {
            .type="application",
            .id="chrome",
            .title="Chrome",
            .volume=0,
            .volumeMax=100,
            .mappedKey=1,
            .keyColor=0x1A2E52,
            .ringPrimary=0x1A2E52,
            .ringSecondary=0x200524,
            .ringPointer=APP_DEV_COLOR_NOT_DEFINED
        },
        {
            .type="application",
            .id="discord",
            .title="Discord",
            .volume=0,
            .volumeMax=100,
            .mappedKey=2,
            .keyColor=0x200524,
            .ringPrimary=0x200524,
            .ringSecondary=0x1A2E52,
            .ringPointer=APP_DEV_COLOR_NOT_DEFINED
        },
        {
            .type="output-device",
            .id="speakers",
            .title="Speakers",
            .volume=0,
            .volumeMax=100,
            .mappedKey=3,
            .keyColor=0x47040D,
            .ringPrimary=0x47040D,
            .ringSecondary=0x08596C,
            .ringPointer=APP_DEV_COLOR_NOT_DEFINED
        }
    };

    std::map<String, devAppInfo *> app_map = {
        { "spotify", &(apps[0]) },
        { "chrome", &(apps[1]) },
        { "discord", &(apps[2]) },
        { "speakers", &(apps[3]) }
    };
};
