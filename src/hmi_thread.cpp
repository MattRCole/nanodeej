#include "hmi_thread.h"
#include "com_thread.h"
#include "foc_thread.h"
#include "utils.h"
#include <SparkFun_STUSB4500.h>

using namespace ace_button;

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Hmi thread controls LED via FastLed and buttons via AceButton

HmiThread::HmiThread(const uint8_t task_core ) : Thread("HMI", 4608, 1, task_core) {
    _q_config_in = xQueueCreate(2, sizeof( ledConfig ));
    _q_hmi_config_in = xQueueCreate(2, sizeof( hmiConfig ));
    _q_settings_in = xQueueCreate(2, sizeof( HmiDeviceSettings ));
    _q_keyevt_out = xQueueCreate(5, sizeof( KeyEvt ));
}

HmiThread::~HmiThread() {}





// init must be called before the thread is started
void HmiThread::init(ledConfig& initial_led_config, hmiConfig& initial_hmi_config) {
    led_config = initial_led_config;
    hmi_config = initial_hmi_config;
    led_max_brightness =  DeviceSettings::getInstance().ledMaxBrightness;
    uint8_t b = min(led_max_brightness, led_config.led_brightness);
    FastLED.setBrightness(b);
};



void HmiThread::put_led_config(ledConfig& new_config) {
    xQueueSend(_q_config_in, &new_config, (TickType_t)0);
};


void HmiThread::put_hmi_config(hmiConfig& new_config){
    xQueueSend(_q_hmi_config_in, &new_config, (TickType_t)0);
};


void HmiThread::put_settings(HmiDeviceSettings& new_settings){
    xQueueSend(_q_settings_in, &new_settings, (TickType_t)0);
};


void HmiThread::handleConfig() {
    ledConfig newConfig;
    if (xQueueReceive(_q_config_in, &newConfig, (TickType_t)0)) {
        led_config = newConfig;
        uint8_t newBrightness = min(led_max_brightness, led_config.led_brightness);
        if (FastLED.getBrightness() != newBrightness) {
            FastLED.setBrightness(newBrightness);
        }
        updateKeyLeds();
    }
    hmiConfig newHmiConfig;
    if (xQueueReceive(_q_hmi_config_in, &newHmiConfig, (TickType_t)0)) {
        hmi_config = newHmiConfig;
    }
};


void HmiThread::handleSettings() {
    HmiDeviceSettings newSettings;
    if (xQueueReceive(_q_settings_in, &newSettings, (TickType_t)0)) {
        led_max_brightness = newSettings.ledMaxBrightness;
        uint8_t newBrightness = min(newSettings.ledMaxBrightness, led_config.led_brightness);
        if (FastLED.getBrightness() != newBrightness) {
            FastLED.setBrightness(newBrightness);
            updateKeyLeds();
        }
        Serial.println("{\"type\":\"debug\",\"msg\":\"Hmi settings updated from global settings\"}");
    }
};


bool HmiThread::get_key_event(KeyEvt* keyEvt){
    return xQueueReceive(_q_keyevt_out, keyEvt, (TickType_t)0);
};




void HmiThread::run() {
    FastLED.addLeds<LED_CHIPSET, PIN_LED_A, RGB>(leds, NANO_LED_A_NUM);
    FastLED.addLeds<LED_CHIPSET, PIN_LED_B, LED_COL_ORDER>(ledsp, NANO_LED_B_NUM);
    FastLED.setBrightness( DEFAULT_LED_MAX_BRIGHTNESS );
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);
    pinMode(PIN_BTN_C, INPUT_PULLUP);
    pinMode(PIN_BTN_D, INPUT_PULLUP);
    buttons[0] = new AceButton(new ButtonConfig(), PIN_BTN_A);
    buttons[1] = new AceButton(new ButtonConfig(), PIN_BTN_B);
    buttons[2] = new AceButton(new ButtonConfig(), PIN_BTN_C);
    buttons[3] = new AceButton(new ButtonConfig(), PIN_BTN_D);
    for (int i = 0; i < 4; i++) {
        button_handler[i] = HmiThreadButtonHandler(i);
        buttons[i]->getButtonConfig()->setIEventHandler(&button_handler[i]);
        buttons[i]->getButtonConfig()->setClickDelay(50);
        buttons[i]->getButtonConfig()->clearFeature(ButtonConfig::kFeatureDoubleClick);
    }
    int keys[4] = {0x1, 0x2, 0x4, 0x8};
    int leds[4][2] = {{3, 4}, {2, 5}, {1, 6}, {0, 7}};
    CRGB colors[4][2] = {
        {led_config.button_A_col_press, led_config.button_A_col_idle},
        {led_config.button_B_col_press, led_config.button_B_col_idle},
        {led_config.button_C_col_press, led_config.button_C_col_idle},
        {led_config.button_D_col_press, led_config.button_D_col_idle}
    };

    for (int i = 0; i < 4; i++) {
        CRGB color = (keyState & keys[i]) ? colors[i][0] : colors[i][1];
        ledsp[leds[i][0]] = color;
        ledsp[leds[i][1]] = color;
    }

    unsigned long total = 0;
    unsigned long updates = 0;
    unsigned long ts = micros();

    while (1) {
        handleSettings();
        handleConfig();
        for (int i = 0; i < 4; i++)
            buttons[i]->check();
        updateValue();
        updateLeds();
        unsigned long currentMillis = millis();
        static unsigned long previousMillis = 0;
        if (currentMillis - previousMillis >= 16) {
            // Limit Leds to ~60fps
            FastLED.show();
            previousMillis = currentMillis;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
};




HmiThreadButtonHandler::HmiThreadButtonHandler(uint8_t _index) : index(_index) {};



void HmiThreadButtonHandler::handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
    switch (eventType) {
        case AceButton::kEventPressed:
            hmi_thread.keyState |= (1<<index);
            for (int i=0; i<hmi_thread.hmi_config.keys[index].num_pressed_actions; i++) {
                hmi_thread.handleKeyAction(hmi_thread.hmi_config.keys[index].pressed[i], eventType);
            }
        break;
        case AceButton::kEventReleased:
            hmi_thread.keyState &= ~(1<<index);
            for (int i=0; i<hmi_thread.hmi_config.keys[index].num_pressed_actions; i++) {
                hmi_thread.handleKeyAction(hmi_thread.hmi_config.keys[index].pressed[i], eventType);
            }            
            for (int i=0; i<hmi_thread.hmi_config.keys[index].num_released_actions; i++) {
                hmi_thread.handleKeyAction(hmi_thread.hmi_config.keys[index].released[i], eventType);
            }
        break;
    }
    KeyEvt keyEvt = { .type=eventType, .keyNum=(uint8_t)index, .keyState=hmi_thread.keyState };
    xQueueSend(hmi_thread._q_keyevt_out, &keyEvt, (TickType_t)0);
    hmi_thread.lastCheck = millis();
    hmi_thread.isIdle = false;
    hmi_thread.last_pos = -1;
    hmi_thread.updateKeyLeds();
};




void HmiThread::handleKeyAction(keyAction& action, uint8_t eventType) {
    StringMessage msg;
    switch (action.type) {
        case keyActionType::KA_PROFILE_CHANGE:
            if (action.profile!="" && eventType==AceButton::kEventPressed) {
                StringMessage msg(new String(action.profile), STRING_MESSAGE_PROFILE);
                com_thread.put_string_message(msg);
            }
        break;
        case keyActionType::KA_PROFILE_NEXT:
            msg = StringMessage(nullptr, STRING_MESSAGE_NEXT_PROFILE);
            if (eventType==AceButton::kEventPressed)
                com_thread.put_string_message(msg);
        break;
        case keyActionType::KA_PROFILE_PREV:
            msg = StringMessage(nullptr, STRING_MESSAGE_PREV_PROFILE);
            if (eventType==AceButton::kEventPressed)
                com_thread.put_string_message(msg);
        break;
    }
};



void HmiThread::updateValue() {
    if (hmi_config.knob.num>0) {
        float angle = foc_thread.get_motor_angle();
        for (int i=0;i<hmi_config.knob.num;i++) {
            knobValue& v = hmi_config.knob.values[i];
            if (v.key_state==keyState) {
                float value = 0;
                if (v.angle_min<v.angle_max) {
                    _constrain(angle, v.angle_min, v.angle_max);
                }
                else {
                    _constrain(angle, v.angle_max, v.angle_min);
                }
                if (v.angle_max == v.angle_min) {
                    value = v.value_min;
                }
                else {
                    value = (angle - v.angle_min) * (v.value_max - v.value_min) / (v.angle_max - v.angle_min) + v.value_min;
                }
                if (v.step!=0) {
                    value = round(value / v.step) * v.step;
                }
                currentValue = value;
                currentValue = foc_thread.pass_cur_pos(); // TODO fix and remove this in future
                if (currentValue!=lastValue) {
                    // No MIDI/HID output - value changes are only used internally
                    lastValue = currentValue;
                }
            }
        } // for over values
    }
};










void HmiThread::updateKeyLeds() {
    int keys[4] = {0x1, 0x2, 0x4, 0x8};
    int leds[4][2] = {{3, 4}, {2, 5}, {1, 6}, {0, 7}};
    CRGB colors[4][2] = {
        {led_config.button_A_col_press, led_config.button_A_col_idle},
        {led_config.button_B_col_press, led_config.button_B_col_idle},
        {led_config.button_C_col_press, led_config.button_C_col_idle},
        {led_config.button_D_col_press, led_config.button_D_col_idle}
    };

    for (int i = 0; i < 4; i++) {
        CRGB color = (keyState & keys[i]) ? colors[i][0] : colors[i][1];
        ledsp[leds[i][0]] = color;
        ledsp[leds[i][1]] = color;
    }
    
};


// Define a variable to store the last time cur_pos was updated

void HmiThread::updateLeds() {
    // TODO: optimise this
    uint16_t cur_pos = foc_thread.pass_cur_pos();
    uint16_t start_pos = foc_thread.pass_start_pos();
    uint16_t end_pos = foc_thread.pass_end_pos();
    uint8_t device_orientation = DeviceSettings::getInstance().deviceOrientation;
    uint8_t led_orientation = map(device_orientation, 0, 3, 0, 135);
    uint16_t point = map(cur_pos, end_pos, start_pos, 0, NANO_LED_A_NUM - 1);
    uint16_t start = map(start_pos, end_pos, start_pos, 0, NANO_LED_A_NUM - 1);
    uint16_t end = map(end_pos, end_pos, start_pos, 0, NANO_LED_A_NUM - 1);


     if (com_thread.global_sleep_flag) {
        hmi_thread.IdleLeds(25, CRGB::Red, CRGB::Green, CRGB::Blue);
        FastLED.setBrightness(25);
    } else {
        halvesPointer(point, start, end, led_orientation, (led_config.pointer_col), CRGB(led_config.primary_col), CRGB(led_config.secondary_col));
        updateKeyLeds();
        FastLED.setBrightness(led_config.led_brightness);
    }
};

    



// Standard Pointer with two halves
void HmiThread::halvesPointer(int indicator, int startpos, int endpos, int orientation, const struct CRGB& pointerCol, const struct CRGB& postCol, const struct CRGB& preCol){ 
    
    for (int i = NANO_LED_A_NUM - 1; i >= 0; i--) {
         if(i > indicator) {
            int index = ( i + orientation) % NANO_LED_A_NUM ;
             leds[index] = postCol;
         }
         if(i < indicator) {
            int index = ( i + orientation) % NANO_LED_A_NUM;
             leds[index] = preCol;
         }
    }
    int index = ( indicator + orientation) % NANO_LED_A_NUM;
    leds[index] = pointerCol;
    return;
};

/*
    IdleLed animation
    Animates the LEDs with a color gradient
*/

static uint8_t colorIndex = 0;
void HmiThread::IdleLeds(int fps, const struct CRGB& idleColStart, const struct CRGB& idleColMid, const struct CRGB& idleColEnd){
    
    CRGB colors[] = {idleColStart ,idleColMid, idleColEnd};
    static unsigned long lastUpdateTime = 0;
    static bool increasing = false;
    static uint8_t darkness = 255;
    static uint8_t progress = 0;
    CRGB beginColor = colors[colorIndex];
    CRGB endColor = colors[(colorIndex + 1) % ARRAY_SIZE(colors)];
    CRGB currentColor = blend(beginColor, endColor, progress);
    for(int i = 0; i < NANO_LED_A_NUM + 8; i++ ) {
    leds[i] = currentColor;
    }
    progress++;
    if (progress == 0) {  // Overflow, time to move to the next color
    colorIndex = (colorIndex + 1) % ARRAY_SIZE(colors);
    }
    if(!com_thread.global_sleep_flag)
    return;
}

STUSB4500 usb_pd;

PowerType HmiThread::init_pd() {
  Wire.begin(PIN_NANO_I2C_SDA, PIN_NANO_I2C_SCL);
  if (!usb_pd.begin()) {
    Serial.println("{\"type\":\"debug\",\"msg\":\"STUSB4500 not found\"}");
  } else {
    Serial.println("{\"type\":\"debug\",\"msg\":\"STUSB4500 found\"}");
  }
  if (usb_pd.getPdoNumber()!=2) {
    Serial.println("{\"type\":\"debug\",\"msg\":\"Setting USB profiles to NVM\"}");
    usb_pd.setUsbCommCapable(true);
    usb_pd.setVoltage(1,5.0);
    usb_pd.setCurrent(1,3.0);
    usb_pd.setLowerVoltageLimit(1,20);
    usb_pd.setUpperVoltageLimit(1,20);
    usb_pd.setVoltage(2,9.0);
    usb_pd.setCurrent(2,3.0);
    usb_pd.setLowerVoltageLimit(2,20);
    usb_pd.setUpperVoltageLimit(2,10);
    usb_pd.setVoltage(3,9.0);
    usb_pd.setCurrent(3,3.0);
    usb_pd.setLowerVoltageLimit(3,20);
    usb_pd.setUpperVoltageLimit(3,10);
    usb_pd.setPdoNumber(2);
    usb_pd.write();  
  }

    // TODO: read status register to determine selected PDO

  return POWER_5V_USB;
}