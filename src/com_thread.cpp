
#include "./com_thread.h"
#include "./foc_thread.h"
#include "./hmi_thread.h"
#include "./lcd_thread.h"
#include <esp_task_wdt.h>
#include "./DeviceSettings.h"
#include "./haptic.h"
#include "default_profiles.h"
#include "utils.h"



ComThread::ComThread(const uint8_t task_core) : Thread("COM", 12000, 1, task_core) {
    _q_strings_in = xQueueCreate(5, sizeof(StringMessage));
};

ComThread::~ComThread() {

};


void ComThread::put_string_message(const StringMessage& msg) {
    xQueueSend(_q_strings_in, &msg, (TickType_t)0);
};



String title = "";
String data1 = "";
String data2 = "";
String data3 = "";
String data4 = "";

void ComThread::run() {
    // serial is initialized in main.cpp, but subsequently used only here
    Serial.println("{\"type\": \"debug\",\"msg\": \"COM thread started\"}");
    unsigned long ts = millis();
    ts_last_activity = ts;
    JsonDocument idleDoc;
    LcdCommand remoteLcdCommand;
    remoteLcdCommand.type = LCD_LAYOUT_DEFAULT;
    remoteLcdCommand.title = &title;
    remoteLcdCommand.data1 = &data1;
    remoteLcdCommand.data2 = &data2;
    remoteLcdCommand.data3 = &data3;
    remoteLcdCommand.data4 = &data4;
    dispatchSettings();
    dispatchLcdConfig();
    dispatchLedConfig();
    while (true) {
        JsonDocument doc;
        if (Serial.available()) {
            String input = Serial.readStringUntil('\n');
            DeserializationError error = deserializeJson(doc, input);
            if (error) {
                doc.clear();
                sendError("JSON parse error", error.c_str());
                continue;
            }
            String messageType = doc["type"].as<String>();
            if (messageType == "app-dev-config") {
                handleAppDevConfigCommand(doc["info"]);
            }
            else if (messageType == "app-dev-key-mapping") {
                handleAppDevKeyMappingCommand(doc["info"]);
            }
            ts_last_activity = millis();
        }

        // send any outgoing messages
        handleMessages();

        // send key events
        handleEvents();

        // send idle message
        unsigned long now = millis();
        if (now - ts > 1000 && now - ts_last_activity > global_idle_timeout && global_idle_timeout > 0) {
            ts = now;
            idleDoc["type"] = "debug";
            idleDoc["idle"] = now - ts_last_activity;
            serializeJson(idleDoc, Serial);
            Serial.println(); // add a newline
        }
        if (now - ts_last_activity <= global_idle_timeout || global_idle_timeout == 0) global_sleep_flag = false;
        else global_sleep_flag = true;

        vTaskDelay(10); // give other threads a chance to run...
    }
};

void ComThread::handleAppDevConfigCommand(JsonVariant info)
{
    JsonArray appDevList = info.as<JsonArray>();
    for (JsonVariant v : appDevList) {
        JsonObject incomingInfo = v.as<JsonObject>();

        uint16_t volume = incomingInfo["currentDetent"].as<uint16_t>();
        uint16_t detentCount = incomingInfo["detents"].as<uint16_t>();
        String appDevName = incomingInfo["name"].as<String>();
        String appDevType = incomingInfo["type"].as<String>();
        String appDevId = incomingInfo["id"].as<String>();

        auto dev = NanoProfiles::apps.find(appDevId);
        if (dev != NanoProfiles::apps.end()) {
            NanoProfiles::devAppInfo &appInfo = dev->second;

            // Note: we're not doing a lot of error checking here.
            // Maybe we want to put some checking in behind some pre-processor flags?
            bool needNewPosition = appInfo.volume != volume;
            bool needDetentUpdate = detentCount != appInfo.volumeMax;
            bool needNameChange = appDevName != appInfo.title;
            bool needTypeChange = appDevType != appInfo.type;
            bool needLedConfigCleared = JSON_IS_NULL(incomingInfo["deejConfig"]);
            bool needLedConfigUpdated = !needLedConfigCleared && !incomingInfo["deejConfig"].isNull();

            bool changingCurrentlyDisplayedApp = appInfo.id == NanoProfiles::keymapped_apps[lastApp];
            if (needDetentUpdate) {
                HapticProfileUpdate hapticConfig;
                hapticConfig.position = volume; // might as well just set it to volume
                hapticConfig.profile = NanoProfiles::default_knob_value.haptic;
                hapticConfig.profile.end_pos = detentCount;

                appInfo.volumeMax = detentCount;
                appInfo.volume = volume; // might as well update this here too just in case it changed.

                foc_thread.put_haptic_config(hapticConfig);
            }
            if (needNewPosition && !needDetentUpdate) { // Don't run this if we've already handled a detent update.
                appInfo.volume = volume;

                if (changingCurrentlyDisplayedApp) foc_thread.put_new_position(volume);
            }

            if (needNameChange) appInfo.title = appDevName;
            if (needTypeChange) appInfo.type = appDevType;
            if (needLedConfigCleared) {
                appInfo.keyColor = APP_DEV_COLOR_NOT_DEFINED;
                appInfo.ringPrimary = APP_DEV_COLOR_NOT_DEFINED;
                appInfo.ringSecondary = APP_DEV_COLOR_NOT_DEFINED;
                appInfo.ringPointer = APP_DEV_COLOR_NOT_DEFINED;
            }
            if (needLedConfigUpdated) {
                JsonObject ledConfig = incomingInfo["deejConfig"].as<JsonObject>();
                // TL;DR if the value is explicitly `null`, reset to undefined
                // otherwise, set the new value if present
                // otherwise, keep existing value
                appInfo.keyColor = JSON_IS_NULL(ledConfig["keyColor"]) ? APP_DEV_COLOR_NOT_DEFINED : JSON_COLOR_DEFAULT_TO_EXISTING(ledConfig["keyColor"], appInfo.keyColor);
                appInfo.ringPrimary = JSON_IS_NULL(ledConfig["ringPrimary"]) ? APP_DEV_COLOR_NOT_DEFINED : JSON_COLOR_DEFAULT_TO_EXISTING(ledConfig["ringPrimary"], appInfo.ringPrimary);
                appInfo.ringSecondary = JSON_IS_NULL(ledConfig["ringSecondary"]) ? APP_DEV_COLOR_NOT_DEFINED : JSON_COLOR_DEFAULT_TO_EXISTING(ledConfig["ringSecondary"], appInfo.ringSecondary);
                appInfo.ringPointer = JSON_IS_NULL(ledConfig["ringPointer"]) ? APP_DEV_COLOR_NOT_DEFINED : JSON_COLOR_DEFAULT_TO_EXISTING(ledConfig["ringPointer"], appInfo.ringPointer);
            }

            if ((needNameChange || needTypeChange) && changingCurrentlyDisplayedApp) dispatchLcdConfig();
            if (needLedConfigCleared || needLedConfigUpdated) dispatchLedConfig();



        }
        else {
            bool useDefaultLEDConfig = JSON_IS_NULL(incomingInfo["deejConfig"]);
            if (useDefaultLEDConfig) {
                NanoProfiles::apps[appDevId] = {
                    .type = appDevType,
                    .id = appDevId,
                    .title = appDevName,
                    .volume = volume,
                    .volumeMax = detentCount,
                    .mappedKey = -1,
                    .keyColor = APP_DEV_COLOR_NOT_DEFINED,
                    .ringPrimary = APP_DEV_COLOR_NOT_DEFINED,
                    .ringSecondary = APP_DEV_COLOR_NOT_DEFINED,
                    .ringPointer = APP_DEV_COLOR_NOT_DEFINED
                };
            }
            else {
                JsonObject ledConfig = incomingInfo["deejConfig"].as<JsonObject>();

                NanoProfiles::apps[appDevId] = {
                    .type = appDevType,
                    .id = appDevId,
                    .title = appDevName,
                    .volume = volume,
                    .volumeMax = detentCount,
                    .mappedKey = -1,
                    .keyColor = JSON_COLOR_DEFAULT_TO_UNDEF(ledConfig["keyColor"]),
                    .ringPrimary = JSON_COLOR_DEFAULT_TO_UNDEF(ledConfig["ringPrimary"]),
                    .ringSecondary = JSON_COLOR_DEFAULT_TO_UNDEF(ledConfig["ringSecondary"]),
                    .ringPointer = JSON_COLOR_DEFAULT_TO_UNDEF(ledConfig["ringPointer "])
                };
            }
        }
    }
};


void ComThread::handleAppDevKeyMappingCommand(JsonVariant info) {
    size_t keyIdx = 0;
    String currentlyDisplayedId = NanoProfiles::keymapped_apps[lastApp];
    JsonArray appDevIds = info.as<JsonArray>();
    bool anyChange = false;
    for (JsonVariant v : appDevIds) {
        if (keyIdx > 3) break; // TODO: There's gotta be a pre-defined max keys somewhere 

        String appId = v.as<String>();
        String prevId = NanoProfiles::keymapped_apps[keyIdx];
        NanoProfiles::keymapped_apps[keyIdx] = appId;

        // If there's any change, we'll re-do the LED config
        if (appId != prevId) anyChange = true;

        keyIdx++;
    }
    if (currentlyDisplayedId != NanoProfiles::keymapped_apps[lastApp]) {
        dispatchLcdConfig();
        dispatchHapticConfig(); // Updates volume and endpos if necessary
    }
    if (anyChange) dispatchLedConfig();
};

void ComThread::handleEvents() {
    JsonDocument eventDoc;
    bool hadEvent = false;
    do {
        KeyEvt keyEvt;
        hadEvent = hmi_thread.get_key_event(&keyEvt);
        if (hadEvent) {
            eventDoc.clear();
            eventDoc["type"] = "debug";
            char keyState[11]; // More than enough to hold 8 bytes of data
            sprintf(keyState, BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(keyEvt.keyState));
            eventDoc["ks"] = keyState;
            if (keyEvt.type == 0) // AceButton::kEventPressed
                eventDoc["kd"] = keyEvt.keyNum;
            else if (keyEvt.type == 1) { // AceButton::kEventReleased
                eventDoc["ku"] = keyEvt.keyNum;
                if (keyEvt.keyNum != lastApp) {
                    NanoProfiles::devAppInfo &appInfo = GET_MAPPED_APP(keyEvt.keyNum);
                    LcdCommand cmd;
                    cmd.type = LCD_LAYOUT_DEFAULT;
                    cmd.title = &appInfo.title;
                    cmd.data1 = &appInfo.type;
                    lcd_thread.put_lcd_command(cmd);

                    HapticProfileUpdate haptic_config;
                    haptic_config.profile = NanoProfiles::default_knob_value.haptic;
                    haptic_config.position = appInfo.volume;
                    foc_thread.put_haptic_config(haptic_config);

                    ledConfig current_led_config = HapticProfileManager::getInstance().getCurrentProfile()->led_config;

                    current_led_config.primary_col = APP_DEV_COLOR_CONF(appInfo, ringPrimary);
                    current_led_config.secondary_col = APP_DEV_COLOR_CONF(appInfo, ringSecondary);
                    current_led_config.pointer_col = APP_DEV_COLOR_CONF(appInfo, ringPointer);
                    hmi_thread.put_led_config(current_led_config);

                    lastApp = keyEvt.keyNum;
                }
            }
            serializeJson(eventDoc, Serial);
            Serial.println(); // add a newline
            ts_last_activity = millis();
        }
    } while (hadEvent);
    do {
        AngleEvt angleEvt;
        NanoProfiles::devAppInfo &appInfo = GET_MAPPED_APP(lastApp);
        hadEvent = foc_thread.get_angle_event(&angleEvt);
        if (!hadEvent) continue;
        eventDoc.clear();
        eventDoc["type"] = "volume-update";
        eventDoc["absolute"] = angleEvt.cur_pos;
        eventDoc["id"] = appInfo.id;
        eventDoc["delta"] = (int16_t)(angleEvt.cur_pos - appInfo.volume);
        appInfo.volume = angleEvt.cur_pos;
        serializeJson(eventDoc, Serial);
        Serial.println(); // add a newline
        ts_last_activity = millis();
    } while (hadEvent);
};

// TODO: Remove this and related hmi code.
void ComThread::handleMessages() {
    StringMessage incoming;
    JsonDocument doc;
    String pName = "";
    if (xQueueReceive(_q_strings_in, &incoming, (TickType_t)0)) {
        HapticProfileManager &pm = HapticProfileManager::getInstance();
        bool sendDoc = false;
        switch (incoming.type) {
        case STRING_MESSAGE_DEBUG:
            if (incoming.message != nullptr) {
                doc["debug"] = *incoming.message;
                sendDoc = true;
            }
            break;
        case STRING_MESSAGE_ERROR:
            if (incoming.message != nullptr) {
                doc["error"] = *incoming.message;
                sendDoc = true;
            }
            break;
        case STRING_MESSAGE_MOTOR:
            if (incoming.message != nullptr) {
                doc["r"] = *incoming.message;
                sendDoc = true;
            }
            break;
        case STRING_MESSAGE_PROFILE:
            if (incoming.message != nullptr) {
                String s = *incoming.message;
                setCurrentProfile(s);
                doc["current"] = s;
                sendDoc = true;
            }
            break;
        case STRING_MESSAGE_NEXT_PROFILE:
            pName = pm.getNextProfileName();
            if (pName != "") {
                setCurrentProfile(pName);
                doc["current"] = pName;
                sendDoc = true;
            }
            break;
        case STRING_MESSAGE_PREV_PROFILE:
            pName = pm.getPrevProfileName();
            if (pName != "") {
                setCurrentProfile(pName);
                doc["current"] = pName;
                sendDoc = true;
            }
            break;
        default:
            if (incoming.message != nullptr) {
                Serial.println(*incoming.message);
            }
            break;
        }
        if (sendDoc) {
            serializeJson(doc, Serial);
            Serial.println(); // add a newline
        }
        if (incoming.message != nullptr) {
            delete incoming.message;
        }
    }
};

void ComThread::sendError(String &error, String *msg) {
    JsonDocument doc;
    doc["error"] = error;
    if (msg != nullptr)
        doc["msg"] = *msg;
    serializeJson(doc, Serial);
    Serial.println(); // add a newline
};
void ComThread::sendError(String &error, String &msg) {
    sendError(error, &msg);
};
void ComThread::sendError(const char *error, String &msg) {
    String e = error;
    sendError(e, &msg);
};
void ComThread::sendError(const char *error, const char *msg) {
    String e = error;
    if (msg == nullptr) {
        sendError(e);
    }
    else {
        String m = msg;
        sendError(e, m);
    }
};



void ComThread::setCurrentProfile(String name) {
    HapticProfile *profile = HapticProfileManager::getInstance().setCurrentProfile(name);
    if (profile != nullptr) { // if we changed profile, send the new haptic config to the FOC thread
        dispatchHapticConfig();
        dispatchLedConfig();
        dispatchHmiConfig();
        dispatchLcdConfig();
    }
};


void ComThread::dispatchLedConfig() {
    ledConfig config;
    config.button_A_col_idle = APP_DEV_COLOR_CONF(GET_MAPPED_APP(0), keyColor);
    config.button_B_col_idle = APP_DEV_COLOR_CONF(GET_MAPPED_APP(1), keyColor);
    config.button_C_col_idle = APP_DEV_COLOR_CONF(GET_MAPPED_APP(2), keyColor);
    config.button_D_col_idle = APP_DEV_COLOR_CONF(GET_MAPPED_APP(3), keyColor);
    config.pointer_col = APP_DEV_COLOR_CONF(GET_MAPPED_APP(lastApp), ringPointer);
    config.primary_col = APP_DEV_COLOR_CONF(GET_MAPPED_APP(lastApp), ringPrimary);
    config.secondary_col = APP_DEV_COLOR_CONF(GET_MAPPED_APP(lastApp), ringSecondary);
    if (config.led_brightness > DeviceSettings::getInstance().ledMaxBrightness)
        config.led_brightness = DeviceSettings::getInstance().ledMaxBrightness;
    hmi_thread.put_led_config(config);
};

void ComThread::dispatchHmiConfig() {
    hmi_thread.put_hmi_config(HapticProfileManager::getInstance().getCurrentProfile()->hmi_config);
};

void ComThread::dispatchHapticConfig() {
    HapticProfileUpdate haptic_config;
    haptic_config.profile = NanoProfiles::default_knob_value.haptic;
    haptic_config.profile.end_pos = GET_MAPPED_APP(lastApp).volumeMax;
    haptic_config.position = GET_MAPPED_APP(lastApp).volume;
    foc_thread.put_haptic_config(haptic_config);
};

void ComThread::dispatchSettings() {
    DeviceSettings &ds = DeviceSettings::getInstance();
    HmiDeviceSettings hmiSettings{
        .ledMaxBrightness = ds.ledMaxBrightness,
        .deviceOrientation = ds.deviceOrientation};
    hmi_thread.put_settings(hmiSettings);
    global_idle_timeout = ds.idleTimeout;
};

void ComThread::dispatchLcdConfig() {
    NanoProfiles::devAppInfo &appInfo = GET_MAPPED_APP(lastApp);
    LcdCommand cmd;
    cmd.type = LCD_LAYOUT_DEFAULT;
    cmd.title = &appInfo.title;
    cmd.data1 = &appInfo.type;
    cmd.data2 = nullptr;
    cmd.data3 = nullptr;
    cmd.data4 = nullptr;
    lcd_thread.put_lcd_command(cmd);
};


int32_t ComThread::cssColorToInt(String color) {
    if (color.length() < 4
    || color.length() == 6
    || color.length() == 8
    || color.length() > 9) {
        // Invalid length
        return APP_DEV_COLOR_NOT_DEFINED;
    }

    if (color.charAt(0) != '#') {
        // kinda draconian but... idk we only support hex encoded colors
        return APP_DEV_COLOR_NOT_DEFINED;
    }


    int32_t *hexMode = (int32_t *)malloc(sizeof(int32_t) * color.length() - 1);

    bool validColor = true;
    for(size_t i = 0; i < (color.length() - 1); i++) {
        char encoded = color.charAt(i + 1);
        if (encoded >= '0' && encoded <= '9') {
            hexMode[i] = 0x0F & encoded;
        }
        else {
            int32_t translated = (0x0F & encoded) + 0b1001;
            if (translated > 0x0F) {
                validColor = false;
                break;
            }
            hexMode[i] = translated;
        }
    }
    if (validColor == false) {
        free(hexMode);
        return APP_DEV_COLOR_NOT_DEFINED;
    }

    int32_t result = APP_DEV_COLOR_NOT_DEFINED;

    if (color.length() == 4 || color.length() == 5) {
        // If using RGBA, we ignore the A.
        result = (hexMode[0] << 20) | (hexMode[0] << 16) | (hexMode[1] << 12) | (hexMode[1] << 8) | (hexMode[2] << 4) | hexMode[2];
    }
    // Logically, these are the only remaining options but...
    else if (color.length() == 7 || color.length() == 9) {
        // If it's 9, we ignore the alpha segment
        result = (hexMode[0] << 20) | (hexMode[1] << 16) | (hexMode[2] << 12) | (hexMode[3] << 8) | (hexMode[4] << 4) | hexMode[5];
    }
    free(hexMode);
    return result;
};
