#include <Arduino.h>

#include "nanofoc_d.h"
#include "./foc_thread.h"
#include "./hmi_thread.h"
#include "./lcd_thread.h"
#include "./com_thread.h"
#include "./DeviceSettings.h"
#include <esp_task_wdt.h>
#include "SPIFFS.h"
#include <Adafruit_TinyUSB.h>

#include <SparkFun_STUSB4500.h>
#include "default_profiles.h"

FocThread foc_thread(1);
HmiThread hmi_thread(0);
LcdThread lcd_thread(0);
ComThread com_thread(0);

STUSB4500 usb;


void setup() {

  // Initialize basic USB device (needed for serial communication)
  TinyUSBDevice.begin();
  TinyUSBDevice.setID(0x239A, 0x8010);
  TinyUSBDevice.setProductDescriptor("Nano_D++ (Beta)");
  TinyUSBDevice.setManufacturerDescriptor("Binaris Circuitry");
  TinyUSBDevice.setSerialDescriptor("Nano_D");

  Serial.begin(DEFAULT_SERIAL_SPEED);

  delay(100);
  Serial.println("{\"type\":\"debug\",\"msg\":\"Welcome to Nano_D++!\"}");
  Serial.print("{\"type\":\"debug\",\"msg\":\"Firmware version: ");
  Serial.print(NANO_FIRMWARE_VERSION);
  Serial.println("\"}");
  Serial.println("{\"type\":\"debug\",\"msg\":\"Initializing...\"}");
  // before we begin, load our global settings...
  DeviceSettings& settings = DeviceSettings::getInstance();
  settings.init();
  settings.fromSPIFFS(); // attempt to load settings from SPIFFS

  // initialize PD power
  hmi_thread.init_pd();

  // then load the profiles
  HapticProfileManager& profileManager = HapticProfileManager::getInstance();
  profileManager.fromSPIFFS(); // attempt to load profiles from SPIFFS

  // load motor calibration from Preferences
  MotorCalibration cal = settings.loadCalibration();
  foc_thread.setCalibration(cal);
  
  // load current profile from Preferences
  String current_profile = settings.loadCurrentProfile();
  profileManager.setCurrentProfile(current_profile);
  NanoProfiles::default_knob_mapping.num = 1;
  NanoProfiles::default_knob_mapping.values[0] = NanoProfiles::default_knob_value;
  NanoProfiles::default_haptic_profile.dirty = false;
  NanoProfiles::default_haptic_profile.gui_enable = true;
  NanoProfiles::default_haptic_profile.hmi_config = NanoProfiles::default_hmi_config;
  NanoProfiles::default_haptic_profile.led_config = NanoProfiles::default_led_config;
  NanoProfiles::default_haptic_profile.profile_name = "default-profile-yall";
  NanoProfiles::default_haptic_profile.profile_tag = "Default Pro-file";

  // init threads
  hmi_thread.init(NanoProfiles::default_led_config, NanoProfiles::default_hmi_config);
  foc_thread.init(NanoProfiles::default_knob_value.haptic);

  // start threads
  Serial.println("{\"type\":\"debug\",\"msg\":\"Starting threads...\"}");
  Serial.flush();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  lcd_thread.begin();
  com_thread.begin();
  hmi_thread.begin();
  foc_thread.begin();
  vTaskDelete(NULL);
}

void loop() {
  // main loop not used...
  vTaskDelay(1000);
}
