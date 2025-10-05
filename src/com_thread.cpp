
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
    _q_strings_in = xQueueCreate(5, sizeof( StringMessage ));
};

ComThread::~ComThread() {

};


void ComThread::put_string_message(const StringMessage& msg){
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
            if (doc["type"].as<String>() == "app-dev-config") {
              handleAppDevConfigCommand(doc["info"]);
            }
            ts_last_activity = millis();
        }

        // send any outgoing messages
        handleMessages();

        // send key events
        handleEvents();

        // send idle message
        unsigned long now = millis();
        if (now-ts>1000 && now-ts_last_activity>global_idle_timeout && global_idle_timeout>0) {
          ts = now;          
          idleDoc["type"] = "debug";
          idleDoc["idle"] = now-ts_last_activity;
          serializeJson(idleDoc, Serial);
          Serial.println(); // add a newline
        }
        if (now-ts_last_activity<=global_idle_timeout || global_idle_timeout==0)
          global_sleep_flag = false;
        else
          global_sleep_flag = true;

        vTaskDelay(10); // give other threads a chance to run...
    }

};


void ComThread::handleAppDevConfigCommand(JsonVariant info) {
    JsonArray appDevList = info.as<JsonArray>();
    for(JsonVariant v : appDevList) {
        JsonObject incomingInfo = v.as<JsonObject>();
        String appDevId = incomingInfo["id"].as<String>();
        // For now: we're not adding apps.
        auto dev = NanoProfiles::app_map.find(appDevId);
        if (dev != NanoProfiles::app_map.end()) {
            NanoProfiles::devAppInfo& appInfo = *dev->second;
            uint16_t volume = incomingInfo["currentDetent"].as<uint16_t>();
            if (appInfo.volume != volume) {
                appInfo.volume = volume;
                if (appInfo.id == NanoProfiles::apps[lastApp].id) {
                    foc_thread.put_new_position(volume);
                }
            }
        } else {
            String devInfo;
            serializeJson(incomingInfo, devInfo);
            Serial.printf("{\"type\":\"debug\",\"msg\":\"Adding new app/devs is not yet supported.\",\"device-info\":%s}\n", devInfo.c_str());
        }
    }
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
        if (keyEvt.type==0) // AceButton::kEventPressed
          eventDoc["kd"] = keyEvt.keyNum;
        else if (keyEvt.type==1) {// AceButton::kEventReleased
          eventDoc["ku"] = keyEvt.keyNum;
          if (keyEvt.keyNum != lastApp) {
            NanoProfiles::devAppInfo& appInfo = NanoProfiles::apps[keyEvt.keyNum];
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

            current_led_config.primary_col = APP_DEV_WITH_DEFAULT(appInfo, ringPrimary);
            current_led_config.secondary_col = APP_DEV_WITH_DEFAULT(appInfo, ringSecondary);
            current_led_config.pointer_col = APP_DEV_WITH_DEFAULT(appInfo, ringPointer);
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
        NanoProfiles::devAppInfo &appInfo = NanoProfiles::apps[lastApp];
        hadEvent = foc_thread.get_angle_event(&angleEvt);
        if (!hadEvent) continue;
        eventDoc.clear();
        eventDoc["type"] = "volume-update";
        eventDoc["absolute"] = angleEvt.cur_pos;
        eventDoc["id"] = NanoProfiles::apps[lastApp].id;
        eventDoc["delta"] = (int16_t)(angleEvt.cur_pos - appInfo.volume);
        NanoProfiles::apps[lastApp].volume = angleEvt.cur_pos;
        serializeJson(eventDoc, Serial);
        Serial.println(); // add a newline
        ts_last_activity = millis();
    } while (hadEvent);
};





void ComThread::handleSettingsCommand(JsonVariant s) {
  if (s.isNull()) return;
  if (s.is<String>()) {
    // send the settings
    JsonDocument doc;
    JsonObject obj = doc["settings"].to<JsonObject>();
    DeviceSettings::getInstance().toJSON(obj);
    serializeJson(doc, Serial);
    Serial.println(); // add a newline
  }
  if (s.is<JsonObject>()) {
    JsonObject obj = s.as<JsonObject>();
    DeviceSettings::getInstance() = obj;
    dispatchSettings();
  }
};



void ComThread::handleMessages() {
  StringMessage incoming;
  JsonDocument doc;
  String pName = "";
  if (xQueueReceive(_q_strings_in, &incoming, (TickType_t)0)) {
    HapticProfileManager& pm = HapticProfileManager::getInstance();
    bool sendDoc = false;
    switch(incoming.type) {
      case STRING_MESSAGE_DEBUG:
        if (incoming.message!=nullptr) {
          doc["debug"] = *incoming.message;
          sendDoc = true;
        }
        break;
      case STRING_MESSAGE_ERROR:
        if (incoming.message!=nullptr) {
          doc["error"] = *incoming.message;
          sendDoc = true;
        }
        break;
      case STRING_MESSAGE_MOTOR:
        if (incoming.message!=nullptr) {
          doc["r"] = *incoming.message;
          sendDoc = true;
        }
        break;
      case STRING_MESSAGE_PROFILE:
        if (incoming.message!=nullptr) {
          String s = *incoming.message;
          setCurrentProfile(s);
          doc["current"] = s;
          sendDoc = true;
        }
        break;
      case STRING_MESSAGE_NEXT_PROFILE:
        pName = pm.getNextProfileName();
        if (pName!="") {
          setCurrentProfile(pName);
          doc["current"] = pName;
          sendDoc = true;
        }
        break;
      case STRING_MESSAGE_PREV_PROFILE:
        pName = pm.getPrevProfileName();
        if (pName!="")  {
          setCurrentProfile(pName);
          doc["current"] = pName;
          sendDoc = true;
        }
        break;
      default:
        if (incoming.message!=nullptr) {
          Serial.println(*incoming.message);
        }
        break;
    }
    if (sendDoc) {
      serializeJson(doc, Serial);
      Serial.println(); // add a newline
    }
    if (incoming.message!=nullptr) {
      delete incoming.message;
    }
  }
};


void ComThread::handleProfilesCommand(JsonVariant p) {
  if (p.isNull()) return;
  HapticProfileManager& pm = HapticProfileManager::getInstance();
  if (p.is<String>()) {
    String s = p.as<String>();
    if (s=="#all") {
      // send the list of all profile names
      JsonDocument doc;
      JsonArray arr = doc["profiles"].to<JsonArray>();
      for (int i=0; i<pm.size(); i++) {
        arr.add(pm[i]->profile_name);
      }
      doc["current"] = pm.getCurrentProfile()->profile_name;
      serializeJson(doc, Serial);
      Serial.println(); // add a newline
    }
  }
  if (p.is<JsonArray>()) {
    JsonArray arr = p.as<JsonArray>();
    for (int i=0; i<MAX_PROFILES; i++) {
      HapticProfile* p = pm[i];
      if (p!=nullptr) {
        bool found = false;
        for (int i=0; i<arr.size(); i++) {
          if (arr[i].is<String>()) {
            String s = arr[i].as<String>();
            if (s==p->profile_name) {
              found = true;
              break;
            }
          }
        }
        if (!found) {
          Serial.println("{\"type\":\"debug\",\"msg\":\"Deleting profile "+p->profile_name+"\"}");
          pm.remove(p->profile_name);
        }
      }
    }
  }
  // TODO reorder profiles
};




bool ComThread::isProfileNameOk(String& name){
  if (name==nullptr)
    return false;
  if (name.length()<1 || name.length()>20)
    return false;
  // TODO check for invalid characters
  return true;
};




void ComThread::sendError(String& error, String* msg){
      JsonDocument doc;
      doc["error"] = error;
      if (msg!=nullptr)
        doc["msg"] = *msg;
      serializeJson(doc, Serial);
      Serial.println(); // add a newline
};
void ComThread::sendError(String& error, String& msg){
  sendError(error, &msg);
};
void ComThread::sendError(const char* error, String& msg) {
  String e = error;
  sendError(e, &msg);
};
void ComThread::sendError(const char* error, const char* msg) {
  String e = error;
  if (msg==nullptr) {
    sendError(e);
  }
  else {
    String m = msg;
    sendError(e, m);
  }
};



void ComThread::setCurrentProfile(String name){
  HapticProfile* profile = HapticProfileManager::getInstance().setCurrentProfile(name);
  if (profile!=nullptr) { // if we changed profile, send the new haptic config to the FOC thread
    dispatchHapticConfig();
    dispatchLedConfig();
    dispatchHmiConfig();
    dispatchLcdConfig();
  }
};


void ComThread::dispatchLedConfig() {
    ledConfig config;
    config.button_A_col_idle = APP_DEV_WITH_DEFAULT(NanoProfiles::apps[0], keyColor);
    config.button_B_col_idle = APP_DEV_WITH_DEFAULT(NanoProfiles::apps[1], keyColor);
    config.button_C_col_idle = APP_DEV_WITH_DEFAULT(NanoProfiles::apps[2], keyColor);
    config.button_D_col_idle = APP_DEV_WITH_DEFAULT(NanoProfiles::apps[3], keyColor);
    config.pointer_col = APP_DEV_WITH_DEFAULT(NanoProfiles::apps[lastApp], ringPointer);
    config.primary_col = APP_DEV_WITH_DEFAULT(NanoProfiles::apps[lastApp], ringPrimary);
    config.secondary_col = APP_DEV_WITH_DEFAULT(NanoProfiles::apps[lastApp], ringSecondary);
    if (config.led_brightness>DeviceSettings::getInstance().ledMaxBrightness)
      config.led_brightness = DeviceSettings::getInstance().ledMaxBrightness;
    hmi_thread.put_led_config(config);
};


void ComThread::dispatchHmiConfig() {
    hmi_thread.put_hmi_config(HapticProfileManager::getInstance().getCurrentProfile()->hmi_config);
};

void ComThread::dispatchHapticConfig() {
  if (HapticProfileManager::getInstance().getCurrentProfile()->hmi_config.knob.num>0) {
    HapticProfileUpdate haptic_config;
    haptic_config.profile = NanoProfiles::default_knob_value.haptic;
    haptic_config.position = NanoProfiles::apps[lastApp].volume;
    foc_thread.put_haptic_config(haptic_config);
  }

};

void ComThread::dispatchSettings() {
    DeviceSettings& ds = DeviceSettings::getInstance();
    HmiDeviceSettings hmiSettings{
      .ledMaxBrightness = ds.ledMaxBrightness,
      .deviceOrientation = ds.deviceOrientation
    };
    hmi_thread.put_settings(hmiSettings);
    global_idle_timeout = ds.idleTimeout;
};


String autoDescription = "";

String ComThread::generateDescription(HapticProfile& curr) {
  String desc = "";
  if (curr.hmi_config.knob.num>0) {
    switch (curr.hmi_config.knob.values[0].type) {
      case knobValueType::KV_ACTIONS:
        desc = "Actions";
        break;
      case knobValueType::KV_DEVICE_PROFILES:
        desc = "Profiles";
        break;
      default:
        desc = "?";
        break;
    }
  }
  else {
    desc = "No Mapping";
  }
  return desc;
};


void ComThread::dispatchLcdConfig() {
    NanoProfiles::devAppInfo &appInfo = NanoProfiles::apps[lastApp];
    LcdCommand cmd;
    cmd.type = LCD_LAYOUT_DEFAULT;
    cmd.title = &appInfo.title;
    cmd.data1 = &appInfo.type;
    cmd.data2 = nullptr;
    cmd.data3 = nullptr;
    cmd.data4 = nullptr;
    lcd_thread.put_lcd_command(cmd);
};
