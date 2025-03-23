#pragma once

#include <Arduino.h>
// Remove the non-existent include
// #include <lora_config.h>

#if defined(USE_LORA_VIA_SPI) && defined(LORA_JAMMER_ENABLED)

// Button pin definitions for jammer control
#define LORA_PAUSE_BUTTON_PIN GPIO_NUM_37  // UP button on M5Stack
#define LORA_EXIT_BUTTON_PIN GPIO_NUM_39   // DOWN button on M5Stack

// Global variables
extern bool loraJammerActive;
extern bool loraJammerPaused;
extern int jammingFrequency;
extern bool sweepFrequency;
extern unsigned long jammerDuration;

// Button control variables
extern gpio_num_t loraPauseButtonPin;
extern gpio_num_t loraExitButtonPin;
extern bool lastPauseButtonState;
extern bool lastExitButtonState;

// Jammer functions - explicitly declare as extern for use in other files
extern bool startLoRaJammer();
extern bool stopLoRaJammer();
extern void updateJammer();
extern void jammerLoop();
extern void runLoRaJammer();
extern void interactiveJamming(); // New interactive mode with pause/exit
extern bool isLoRaJammerActive();
extern bool isLoRaJammerPaused();

#endif // USE_LORA_VIA_SPI && LORA_JAMMER_ENABLED 