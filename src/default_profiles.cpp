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


    devAppInfo apps[4] = {
        {.type="Application", .title="Spotify", .volume=0, .volumeMax=100},
        {.type="Application", .title="Chrome", .volume=0, .volumeMax=100},
        {.type="Application", .title="Discord", .volume=0, .volumeMax=100},
        {.type="Output Device", .title="Speakers", .volume=0, .volumeMax=100}
    };
};
