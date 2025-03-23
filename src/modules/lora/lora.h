#ifndef __LORA_H__
#define __LORA_H__

#include <Arduino.h>
#include <SPI.h>
#include <globals.h>
#include "core/config.h"
#include <inttypes.h>

// Default LoRa settings for normal operation
#define DEFAULT_LORA_FREQUENCY 433.0
#define DEFAULT_LORA_TX_POWER 17
#define DEFAULT_LORA_BANDWIDTH 125000
#define DEFAULT_LORA_SPREADING_FACTOR 7
#define DEFAULT_LORA_CODING_RATE 5

// Functions to initialize and control LoRa module
bool initLoRaModule();
bool initLoRaOnce();
bool setLoraFrequency(float frequency);
bool setLoraBandwidth(float bandwidth);
bool setLoraSpreadingFactor(int sf);
bool setLoraTxPower(int power);

// LoRa message transmission/reception
bool sendLoraMessage(const String& message);
String receiveLoraMessage();
bool isLoraMessageAvailable();

// LoRa jammer functionality - these functions are defined in jamming.cpp
// We forward declare them for external use
void runLoRaJammerWrapper(); // Wrapper to call runLoRaJammer from jamming.cpp

// Get module name
String getLoraModuleName(LoRaModules module);

// LoRa settings functions
void setLoraTxPin();
void setLoraRxPin();
void setLoraSckPin();
void setLoraCSPin();
void setLoraResetPin();
void setLoraDIO0Pin();
void setLoraFrequencyMenu();
void setLoraBandwidthMenu();
void setLoraSpreadingFactorMenu();
void setLoraTxPowerMenu();
void setLoraModuleMenu();
void setLoraNickname();
void setLoraJammerSettings();

// Test functions
void runLoRaTest();
bool testLoRaSend();
bool testLoRaReceive();

// Global variables
extern bool loraInitialized;
// loraJammerActive is defined in jamming.h
extern unsigned long loraJammerStartTime;

#endif 