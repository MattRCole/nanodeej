#pragma once

#include <Arduino.h>
#include <ArduinoJSON.h>

// Abstract message transport used by com_thread to send/receive JSON payloads.
class IMessageTransport {
public:
    virtual ~IMessageTransport() {}

    // Initialize underlying transport (non-blocking).
    virtual void begin() = 0;

    // Receive a complete JSON message as a String.
    // Returns true if a full message was received and stored in 'out'.
    virtual bool receive(String& out) = 0;

    // Send a JSON document (adds a trailing newline if applicable).
    virtual void sendJson(JsonDocument& doc) = 0;

    // Optional: send a raw string (debug or plain text line).
    virtual void sendRaw(const String& line) = 0;
};


// CDC-ACM Serial transport implementation.
class CdcSerialTransport : public IMessageTransport {
public:
    void begin() override {
        // Serial is already begun in setup(). No-op.
    }

    bool receive(String& out) override {
        if (Serial.available()) {
            out = Serial.readStringUntil('\n');
            out.trim();
            if (out.length() > 0) return true;
        }
        return false;
    }

    void sendJson(JsonDocument& doc) override {
        serializeJson(doc, Serial);
        Serial.println();
    }

    void sendRaw(const String& line) override {
        Serial.println(line);
    }
};


// Placeholder for future HID transport (not yet wired).
class HidTransport : public IMessageTransport {
public:
    void begin() override {}
    bool receive(String& out) override { (void)out; return false; }
    void sendJson(JsonDocument& doc) override { (void)doc; }
    void sendRaw(const String& line) override { (void)line; }
};


