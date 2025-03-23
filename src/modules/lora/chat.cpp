#include "chat.h"
#include "lora.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/mykeyboard.h"
#include <time.h>
#include <vector>
#include <ArduinoJson.h>

// Define button reading constant
#define BUTTON_PRESSED_READING LOW

std::vector<LoraMessage> loraMessages;
const int MAX_LORA_MESSAGES = 100;
bool loraChatActive = false;

// Background scanning variables
#if defined(USE_LORA_VIA_SPI) && defined(LORA_BACKGROUND_SCAN)
bool loraBackgroundScan = true;
#else
bool loraBackgroundScan = false;
#endif

#ifdef LORA_EXIT_KEY_PIN
gpio_num_t loraExitKeyPin = (gpio_num_t)LORA_EXIT_KEY_PIN;
#else
gpio_num_t loraExitKeyPin = GPIO_NUM_39; // Default to DOWN button
#endif

#ifdef LORA_EXIT_KEY_HOLD_TIME
unsigned long loraExitKeyHoldTime = LORA_EXIT_KEY_HOLD_TIME;
#else
unsigned long loraExitKeyHoldTime = 2000; // Default 2 seconds
#endif

unsigned long loraExitKeyPressStartTime = 0;
bool loraKeyPressed = false;

// Start the LoRa chat room interface
void startLoraChatroom() {
#if defined(USE_LORA_VIA_SPI)
    // Load previous messages if available
    loadLoraMessages();
    
    loraChatActive = true;
    bool exitChat = false;
    unsigned long lastCheckTime = 0;
    unsigned long lastDisplayRefresh = 0;
    bool countdownComplete = false;
    
    tft.fillScreen(bruceConfig.bgColor);
    drawMainBorder(false);
    
    // Display header
    tft.fillRect(0, 0, tftWidth, 25, bruceConfig.priColor);
    tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
    tft.setTextSize(FM);
    tft.drawCentreString("LoRa Chat", tftWidth / 2, 5, 1);
    
    // Display chat navigation help
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Enter: Type | Up/Down: Scroll | Hold to Exit", tftWidth / 2, tftHeight - 15, 1);
    
    // Initial display of messages
    int currentMessageIndex = 0;
    refreshChatDisplay(currentMessageIndex);
    
    while (loraChatActive && !exitChat) {
        // Check for exit key hold
        if (digitalRead(loraExitKeyPin) == BUTTON_PRESSED_READING) {
            if (!loraKeyPressed) {
                loraKeyPressed = true;
                loraExitKeyPressStartTime = millis();
                countdownComplete = false;
                
                // Refresh immediately to show countdown
                refreshChatDisplay(currentMessageIndex);
            } else {
                unsigned long elapsedTime = millis() - loraExitKeyPressStartTime;
                
                // Check for timer to exit
                if (elapsedTime >= loraExitKeyHoldTime) {
                    // Exit the chat when the key is held long enough
                    exitChat = true;
                }
                
                // Only refresh if we're not in the final moment of countdown
                if (!countdownComplete) {
                    // Check if we're at the last second
                    if (elapsedTime >= (loraExitKeyHoldTime - 200)) {
                        countdownComplete = true;
                    }
                    
                    // Refresh display every 100ms to update countdown
                    if (millis() - lastDisplayRefresh > 100) {
                        lastDisplayRefresh = millis();
                        refreshChatDisplay(currentMessageIndex);
                    }
                }
            }
        } else {
            if (loraKeyPressed) {
                loraKeyPressed = false;
                countdownComplete = false;
                // Refresh to remove countdown
                refreshChatDisplay(currentMessageIndex);
            }
        }
        
        // Check for incoming messages every 250ms
        if (millis() - lastCheckTime > 250) {
            lastCheckTime = millis();
            
            if (isLoraMessageAvailable()) {
                String message = receiveLoraMessage();
                if (message.length() > 0) {
                    saveLoraMessage(message, false);
                    refreshChatDisplay(currentMessageIndex);
                }
            }
        }
        
        // Handle user input for keyboard
        if (check(SelPress)) {
            // Open keyboard for sending a message
            String message = keyboard("", 240, "Type your message:");
            if (message.length() > 0) {
                String nickname = bruceConfig.loraNickname;
                if (nickname.length() == 0) {
                    nickname = "User" + String(random(1000, 9999));
                    bruceConfig.loraNickname = nickname;
                    bruceConfig.saveFile();
                }
                
                String fullMessage = nickname + ": " + message;
                sendLoraMessage(fullMessage);
                saveLoraMessage(fullMessage, true);
                
                // Reset to view latest message
                currentMessageIndex = 0;
                refreshChatDisplay(currentMessageIndex);
            }
            
            // Redraw interface after keyboard closes
            tft.fillScreen(bruceConfig.bgColor);
            drawMainBorder(false);
            tft.fillRect(0, 0, tftWidth, 25, bruceConfig.priColor);
            tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
            tft.setTextSize(FM);
            tft.drawCentreString("LoRa Chat", tftWidth / 2, 5, 1);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.drawCentreString("Enter: Type | Up/Down: Scroll | Hold to Exit", tftWidth / 2, tftHeight - 15, 1);
            refreshChatDisplay(currentMessageIndex);
        }
        
        // Handle scrolling with UP/DOWN buttons
        if (check(UpPress) || check(PrevPress)) {
            // Scroll up to older messages
            if (currentMessageIndex < loraMessages.size() - 1) {
                currentMessageIndex++;
                refreshChatDisplay(currentMessageIndex);
            }
        }
        
        if (check(DownPress) || check(NextPress)) {
            // Scroll down to newer messages
            if (currentMessageIndex > 0) {
                currentMessageIndex--;
                refreshChatDisplay(currentMessageIndex);
            }
        }
        
        // Run the jammer if it's active
        runLoRaJammerWrapper();
        
        delay(50);
        
        // Check if we should return to LoRa menu instead of main menu
        if (exitChat) {
            loraChatActive = false;
            // Return to the LoRa menu (the calling function)
            return;
        }
    }
#else
    // Minimal implementation for other boards
    tft.fillScreen(bruceConfig.bgColor);
    drawMainBorder(false);
    tft.fillRect(0, 0, tftWidth, 25, bruceConfig.priColor);
    
    // Draw header
    tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
    tft.setTextSize(FM);
    tft.drawCentreString("LoRa Chat not implemented yet", tftWidth / 2, tftHeight / 2, 1);
    
    delay(3000);
#endif
}

// Save a LoRa message
void saveLoraMessage(const String& message, bool fromMe) {
    LoraMessage newMsg;
    newMsg.content = message;
    newMsg.timestamp = millis();
    newMsg.fromMe = fromMe;
    
    loraMessages.push_back(newMsg);
    
    // Limit stored messages
    if (loraMessages.size() > MAX_LORA_MESSAGES) {
        loraMessages.erase(loraMessages.begin());
    }
    
#if defined(USE_LORA_VIA_SPI)
    // Save messages to file
    File file;
    if (LittleFS.exists("/lora_chat.json")) {
        LittleFS.remove("/lora_chat.json");
    }
    
    file = LittleFS.open("/lora_chat.json", "w");
    if (!file) {
        Serial.println("Failed to open lora_chat.json for writing");
        return;
    }
    
    DynamicJsonDocument doc(16384); // Adjust size as needed
    JsonArray messagesArray = doc.createNestedArray("messages");
    
    for (const auto& msg : loraMessages) {
        JsonObject messageObj = messagesArray.createNestedObject();
        messageObj["content"] = msg.content;
        messageObj["timestamp"] = msg.timestamp;
        messageObj["fromMe"] = msg.fromMe;
    }
    
    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write to lora_chat.json");
    }
    
    file.close();
#endif
}

// Load previously saved LoRa messages
bool loadLoraMessages() {
#if defined(USE_LORA_VIA_SPI)
    loraMessages.clear();
    
    if (!LittleFS.exists("/lora_chat.json")) {
        return false;
    }
    
    File file = LittleFS.open("/lora_chat.json", "r");
    if (!file) {
        Serial.println("Failed to open lora_chat.json for reading");
        return false;
    }
    
    DynamicJsonDocument doc(16384); // Adjust size as needed
    DeserializationError error = deserializeJson(doc, file);
    
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        file.close();
        return false;
    }
    
    JsonArray messagesArray = doc["messages"];
    
    for (JsonObject messageObj : messagesArray) {
        LoraMessage message;
        message.content = messageObj["content"].as<String>();
        message.timestamp = messageObj["timestamp"].as<unsigned long>();
        message.fromMe = messageObj["fromMe"].as<bool>();
        
        loraMessages.push_back(message);
    }
    
    file.close();
    return true;
#else
    // Just return true for compilation on other platforms
    return true;
#endif
}

// View all LoRa messages
void viewLoraMessages() {
#if defined(USE_LORA_VIA_SPI)
    loadLoraMessages();
    
    if (loraMessages.empty()) {
        showMessageDialog("No Messages", "You don't have any LoRa messages yet.", "OK", false);
        delay(1000);
        return;
    }
    
    tft.fillScreen(bruceConfig.bgColor);
    drawMainBorder(false);
    tft.fillRect(0, 0, tftWidth, 25, bruceConfig.priColor);
    
    // Draw header
    tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
    tft.setTextSize(FM);
    tft.drawCentreString("LoRa Messages", tftWidth / 2, 5, 1);
    
    // Draw instructions
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("UP/DOWN to scroll, ESC to exit", tftWidth / 2, tftHeight - 15, 1);
    
    int currentIndex = 0;
    int totalMessages = loraMessages.size();
    
    displayLoraMessage(loraMessages[currentIndex], currentIndex, totalMessages);
    
    bool exitView = false;
    
    while (!exitView && !returnToMenu) {
        if (check(UpPress) || check(PrevPress)) {
            if (currentIndex > 0) {
                currentIndex--;
                displayLoraMessage(loraMessages[currentIndex], currentIndex, totalMessages);
            }
        }
        
        if (check(DownPress) || check(NextPress)) {
            if (currentIndex < totalMessages - 1) {
                currentIndex++;
                displayLoraMessage(loraMessages[currentIndex], currentIndex, totalMessages);
            }
        }
        
        if (check(EscPress)) {
            exitView = true;
        }
        
        delay(50);
    }
#else
    // Minimal implementation for other boards
    if (loraMessages.empty()) {
        showMessageDialog("No Messages", "You don't have any LoRa messages yet.", "OK", false);
        delay(1000);
        return;
    }
    
    tft.fillScreen(bruceConfig.bgColor);
    drawMainBorder(false);
    tft.fillRect(0, 0, tftWidth, 25, bruceConfig.priColor);
    
    // Draw header
    tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
    tft.setTextSize(FM);
    tft.drawCentreString("LoRa Messages", tftWidth / 2, 5, 1);
    
    // Show message count
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("Messages: " + String(loraMessages.size()), 
                        tftWidth / 2, tftHeight / 2, 1);
    
    delay(3000);
#endif
}

// Get the count of LoRa messages
int getLoraMessageCount() {
    return loraMessages.size();
}

// Clear all LoRa messages
void clearLoraMessages() {
    loraMessages.clear();
    
#if defined(USE_LORA_VIA_SPI)
    if (LittleFS.exists("/lora_chat.json")) {
        LittleFS.remove("/lora_chat.json");
    }
#endif
}

// Display a LoRa message
void displayLoraMessage(LoraMessage& message, int index, int totalMessages) {
#if defined(USE_LORA_VIA_SPI)
    // Clear message area
    tft.fillRect(0, 25, tftWidth, tftHeight - 40, bruceConfig.bgColor);
    
    // Display message index
    String indexStr = String(index + 1) + "/" + String(totalMessages);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);
    tft.drawString(indexStr, 5, 30, 1);
    
    // Show if message was sent or received
    String directionStr = message.fromMe ? "Sent" : "Received";
    tft.drawRightString(directionStr, tftWidth - 5, 30, 1);
    
    // Limit content length to prevent buffer overflow
    String content = message.content;
    if (content.length() > 256) { // Limit to reasonable size
        content = content.substring(0, 256) + "...";
    }
    
    // Display message content with word wrap
    tft.setTextSize(FM);
    tft.setTextColor(message.fromMe ? bruceConfig.priColor : 0x07E0, bruceConfig.bgColor);
    
    // Simple word wrap implementation with bounds checking
    int y = 50;
    int charWidth = 6; // Approximate character width
    int charsPerLine = (tftWidth - 10) / charWidth;
    if (charsPerLine <= 0) charsPerLine = 1; // Prevent division by zero
    
    int maxLines = (tftHeight - 70) / 16; // Max visible lines
    int currentLine = 0;
    
    for (int i = 0; i < content.length() && currentLine < maxLines; i += charsPerLine, currentLine++) {
        String line;
        if (i + charsPerLine < content.length()) {
            line = content.substring(i, i + charsPerLine);
        } else {
            line = content.substring(i);
        }
        tft.drawString(line, 5, y, 1);
        y += 16; // Line height
        
        // Break if we're running out of vertical space
        if (y > tftHeight - 35) break;
    }
#else
    // Minimal implementation for compilation
#endif
}

// Checks for background messages when not in chat
bool checkBackgroundMessages() {
#if defined(USE_LORA_VIA_SPI) && defined(LORA_BACKGROUND_SCAN)
    if (!loraChatActive && loraBackgroundScan) {
        if (isLoraMessageAvailable()) {
            String message = receiveLoraMessage();
            if (message.length() > 0) {
                saveLoraMessage(message, false);
                return true;
            }
        }
    }
#endif
    return false;
}

// Handle key hold for exiting with countdown
// This function is now integrated directly in startLoraChatroom
// Keeping this as a stub for backward compatibility
void handleKeyHold(gpio_num_t key, unsigned long holdTime) {
#if defined(USE_LORA_VIA_SPI)
    // Functionality moved to startLoraChatroom
#endif
}

// LoRa chat main loop - enhanced to handle background scanning
void loraChatLoop() {
#if defined(USE_LORA_VIA_SPI)
    static unsigned long lastAttemptTime = 0;
    static bool firstFailure = true;
    const unsigned long reconnectInterval = 15000; // 15 seconds between reconnection attempts
    
    // Attempt to initialize LoRa if needed, with rate limiting
    if (!loraInitialized) {
        unsigned long currentTime = millis();
        
        // Only try to initialize occasionally to prevent freezing
        if (currentTime - lastAttemptTime > reconnectInterval) {
            // Just try once, the initLoRaOnce function has its own retry limits
            initLoRaOnce();
            lastAttemptTime = currentTime;
            
            // If still not initialized, we'll try again later
            if (!loraInitialized && firstFailure) {
                firstFailure = false;
            }
        }
        
        // If not initialized after attempts, just skip this cycle
        if (!loraInitialized) {
            return;
        }
    }
    
    // Now safe to use LoRa functions as we're initialized
    try {
        // Handle active chat logic
        if (loraChatActive) {
            if (isLoraMessageAvailable()) {
                String message = receiveLoraMessage();
                if (message.length() > 0) {
                    saveLoraMessage(message, false);
                    refreshChatDisplay();
                }
            }
        } 
        // Handle background scanning
        else if (loraBackgroundScan) {
            checkBackgroundMessages();
        }
        
        // Run jammer if active
        runLoRaJammerWrapper();
    }
    catch (...) {
        // If any exception during LoRa operations, mark as uninitialized
        // but don't try to immediately reconnect (will wait for next cycle)
        loraInitialized = false;
        lastAttemptTime = millis(); // Update time to ensure we wait before retry
    }
#endif
}

// Refresh the chat display
void refreshChatDisplay(int currentMessageIndex) {
#if defined(USE_LORA_VIA_SPI)
    // Clear screen except top header
    tft.fillRect(0, 25, tftWidth, tftHeight - 40, bruceConfig.bgColor);
    
    if (loraMessages.size() == 0) {
        // No messages
        tft.setTextColor(bruceConfig.priColor);
        tft.setTextSize(1);
        tft.drawCentreString("No messages", tftWidth / 2, tftHeight / 2, 1);
        return;
    }
    
    // Bounds check
    int messagesCount = loraMessages.size();
    if (currentMessageIndex < 0) currentMessageIndex = 0;
    if (currentMessageIndex >= messagesCount) currentMessageIndex = messagesCount - 1;
    
    // Display current message
    try {
        LoraMessage& message = loraMessages[messagesCount - 1 - currentMessageIndex];
        
        // Display message index
        String indexStr = String(currentMessageIndex + 1) + "/" + String(messagesCount);
        tft.setTextColor(bruceConfig.priColor);
        tft.setTextSize(1);
        tft.drawString(indexStr, 5, 30, 1);
        
        // Show exit timer in the middle if a key is being held
        if (loraKeyPressed && (millis() - loraExitKeyPressStartTime > 300)) {
            unsigned long elapsedTime = millis() - loraExitKeyPressStartTime;
            int totalSeconds = loraExitKeyHoldTime / 1000;
            int remainingTime = (loraExitKeyHoldTime - elapsedTime) / 1000 + 1;
            
            if (remainingTime > 0) {
                // Format as "EXIT IN 1/3" to match power button style
                String countText = "EXIT IN " + String(totalSeconds - remainingTime + 1) + "/" + String(totalSeconds);
                tft.setTextColor(TFT_RED);
                tft.drawCentreString(countText, tftWidth / 2, 30, 1);
            }
        }
        
        // Show sender/recipient
        String directionStr = message.fromMe ? "Sent" : "Received";
        tft.setTextColor(bruceConfig.priColor);
        tft.drawRightString(directionStr, tftWidth - 5, 30, 1);
        
        // Display message content with length limit
        String content = message.content;
        if (content.length() > 256) {
            content = content.substring(0, 256) + "...";
        }
        
        // Draw message in a speech bubble style
        int bubblePadding = 5;
        int textY = 50;
        int charWidth = 6; // Approximate width per character
        int maxBubbleWidth = tftWidth - 20;
        int bubbleWidth = min(maxBubbleWidth, (int)(content.length() * charWidth));
        
        if (bubbleWidth < 20) bubbleWidth = 20; // Minimum bubble width
        int rectX = message.fromMe ? tftWidth - bubbleWidth - 10 : 5;
        if (rectX < 0) rectX = 0; // Prevent negative position
        
        // Calculate bubble height based on content
        int charsPerLine = (bubbleWidth / charWidth);
        if (charsPerLine <= 0) charsPerLine = 1; // Prevent division by zero
        int lines = (content.length() / charsPerLine) + 1;
        int bubbleHeight = min(lines * 16 + bubblePadding * 2, tftHeight - textY - 20);
        
        // Draw the message bubble
        tft.fillRoundRect(rectX - bubblePadding, textY - bubblePadding, 
                          bubbleWidth + bubblePadding * 2, bubbleHeight,
                          5, message.fromMe ? 0x6B4D : 0x0300);
        
        // Draw the message text
        tft.setTextSize(1);
        tft.setTextColor(bruceConfig.bgColor);
        
        // Simple word wrap with bounds checking
        int startPos = 0;
        int yOffset = 0;
        int lineWidth = bubbleWidth - 10;
        int charsPerBubbleLine = max(1, lineWidth / charWidth);
        int maxLines = (bubbleHeight - bubblePadding * 2) / 16;
        int lineCount = 0;
        
        while (startPos < content.length() && lineCount < maxLines) {
            String line;
            if (startPos + charsPerBubbleLine < content.length()) {
                line = content.substring(startPos, startPos + charsPerBubbleLine);
            } else {
                line = content.substring(startPos);
            }
            
            tft.drawString(line, rectX, textY + yOffset, 1);
            startPos += charsPerBubbleLine;
            yOffset += 16; // Line height
            lineCount++;
            
            // Break if we're running out of space
            if (textY + yOffset > tftHeight - 30) break;
        }
    } catch (...) {
        // Handle any exceptions during rendering
        tft.setTextColor(0xF800); // Red error color
        tft.setTextSize(1);
        tft.drawCentreString("Error displaying message", tftWidth / 2, tftHeight / 2, 1);
    }
#endif
} 