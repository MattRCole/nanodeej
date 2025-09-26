#pragma once

#include <Arduino.h>
#include "thread_crtp.h"
#include <HardwareSerial.h>
#include <MIDI.h>
#include <ArduinoJSON.h>
#include "HapticProfileManager.h"
#include "transport.h"


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
        bool isProfileNameOk(String& name);
        
        bool global_sleep_flag = false;
        unsigned long ts_last_activity;
        uint32_t global_idle_timeout = 5000;

    protected:
        void run();
        void handleProfileCommand(JsonVariant profile, JsonVariant updates);
        void handleSettingsCommand(JsonVariant s);
        void handleProfilesCommand(JsonVariant p);
        void handleMessages();
        void handleEvents();

        // Mode/state and chord handling
        enum DeviceMode { MODE_VOLUME, MODE_OUTPUT, MODE_INPUT, MODE_WILDCARD };
        DeviceMode currentMode = MODE_VOLUME;
        void sendModeEnter(DeviceMode m);
        void sendMuteToggle();
        void sendVolumeSlot(uint8_t slot);
        void sendDial(uint16_t value);
        void sendSelectIndex(uint16_t index);
        void sendConfirm(uint16_t index);
        const char* modeName(DeviceMode m);
        void updateLcdText(const char* titleOpt, const char* data1Opt);
        void showOverlay(const char* text);

        // chord detection
        uint8_t pressedMask = 0;             // bit i set when key i is pressed
        uint32_t chordWindowMs = 200;        // window to accept a chord
        uint32_t chordLockoutMs = 500;       // prevent immediate repeats
        uint32_t chordStartMs = 0;           // first press time in a sequence
        uint32_t chordLastFireMs = 0;        // last time a chord fired
        bool pendingSingle = false;          // waiting to resolve single vs chord
        uint8_t pendingKey = 0xFF;           // candidate single key
        void tryResolveChordOrSingle(uint32_t nowMs);

        // Dial/list state provided by host
        uint16_t dialMin = 0;
        uint16_t dialMax = 100;
        uint16_t dialStep = 1;
        uint16_t dialValue = 0;
        uint16_t listCount = 0;
        uint16_t listIndex = 0;
        uint16_t lastDialSent = 65535;
        uint16_t lastIndexSent = 65535;
        void applyDialHaptics();
        void applyListHaptics();

        void dispatchLedConfig();
        void dispatchHapticConfig();
        void dispatchHmiConfig();
        void dispatchSettings();
        void dispatchAudioConfig();
        void dispatchLcdConfig();

        String generateDescription(HapticProfile& curr);

        void sendError(String& error, String* msg = nullptr);
        void sendError(String& error, String& msg);
        void sendError(const char* error, String& msg);
        void sendError(const char* error, const char* msg = nullptr);

        QueueHandle_t _q_strings_in;

        // Transport abstraction (defaults to CDC Serial)
        IMessageTransport* transport = nullptr;
        CdcSerialTransport cdcTransport;
};


extern ComThread com_thread;