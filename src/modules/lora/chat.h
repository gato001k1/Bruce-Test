#ifndef __LORA_CHAT_H__
#define __LORA_CHAT_H__

#include <Arduino.h>
#include <globals.h>

// Message structure
struct LoraMessage {
    String content;
    unsigned long timestamp;
    bool fromMe;
};

// Functions for chatroom
void startLoraChatroom();
void saveLoraMessage(const String& message, bool fromMe = true);
void viewLoraMessages();
bool loadLoraMessages();
int getLoraMessageCount();

// Utility functions
void clearLoraMessages();
void displayLoraMessage(LoraMessage& message, int index, int totalMessages);
void loraChatLoop();
void refreshChatDisplay(int currentMessageIndex = 0);
bool checkBackgroundMessages();
void handleKeyHold(gpio_num_t key, unsigned long holdTime);

// Background scanning settings
extern bool loraBackgroundScan;
extern gpio_num_t loraExitKeyPin;
extern unsigned long loraExitKeyHoldTime;
extern unsigned long loraExitKeyPressStartTime;
extern bool loraKeyPressed;

extern std::vector<LoraMessage> loraMessages;
extern const int MAX_LORA_MESSAGES;
extern bool loraChatActive;

#endif 