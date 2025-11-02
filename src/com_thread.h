#pragma once

#include <Arduino.h>
#include "thread_crtp.h"
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include "HapticProfileManager.h"


#define JSON_COLOR_DEFAULT_TO_UNDEF(json_variant) (json_variant.isNull() ? APP_DEV_COLOR_NOT_DEFINED : cssColorToInt(json_variant.as<String>()))
#define JSON_COLOR_DEFAULT_TO_EXISTING(json_variant, existing_color) (json_variant.isNull() ? existing_color : cssColorToInt(json_variant.as<String>()))

enum StringMessageType {
    STRING_MESSAGE_DEBUG,
    STRING_MESSAGE_ERROR,
    STRING_MESSAGE_MOTOR,
    STRING_MESSAGE_PROFILE,
    STRING_MESSAGE_NEXT_PROFILE,
    STRING_MESSAGE_PREV_PROFILE
};

class StringMessage {
    public:
        StringMessage(String* message = nullptr, StringMessageType type = StringMessageType::STRING_MESSAGE_DEBUG) : message(message),  type(type) {};
        String* message;// = nullptr;
        StringMessageType type;
};



class ComThread : public Thread<ComThread> {
    friend class Thread<ComThread>; //Allow Base Thread to invoke protected run()
    public:
        ComThread(const uint8_t task_core);
        ~ComThread();

        void setCurrentProfile(String name);
        void put_string_message(const StringMessage& msg);
        
        bool global_sleep_flag = false;
        unsigned long ts_last_activity;
        uint8_t lastApp = 0;
        uint32_t global_idle_timeout = 5000;

    protected:
        void run();
        void handleProfileCommand(JsonVariant profile, JsonVariant updates);
        void handleAppDevConfigCommand(JsonVariant info);
        void handleAppDevKeyMappingCommand(JsonVariant info);
        void handleMessages();
        void handleEvents();

        void dispatchLedConfig();
        void dispatchHapticConfig();
        void dispatchHmiConfig();
        void dispatchSettings();
        void dispatchLcdConfig();
        int32_t cssColorToInt(String color);

        void sendError(String& error, String* msg = nullptr);
        void sendError(String& error, String& msg);
        void sendError(const char* error, String& msg);
        void sendError(const char* error, const char* msg = nullptr);

        QueueHandle_t _q_strings_in;
};


extern ComThread com_thread;