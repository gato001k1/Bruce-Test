#include "jamming.h"
#include "core/display.h"
#include <LoRa.h>
#include "lora.h" // Add missing include for initLoRaModule

#if defined(USE_LORA_VIA_SPI) && defined(LORA_JAMMER_ENABLED)

// Global variables
bool loraJammerActive = false;
bool loraJammerPaused = false;
int jammingFrequency = 433; // Default jamming frequency (MHz)
bool sweepFrequency = false;
unsigned long jammerStartTime = 0;
unsigned long jammerDuration = 30000; // Default 30 seconds
int sweepStep = 0;
int sweepDirection = 1;
const int sweepMin = 400; // 400 MHz
const int sweepMax = 500; // 500 MHz
const int sweepIncrement = 5; // 5 MHz steps

// Button control variables
gpio_num_t loraPauseButtonPin = LORA_PAUSE_BUTTON_PIN;
gpio_num_t loraExitButtonPin = LORA_EXIT_BUTTON_PIN;
bool lastPauseButtonState = HIGH; // Assume active LOW buttons
bool lastExitButtonState = HIGH;
bool lastSelectButtonState = HIGH;
unsigned long lastButtonDebounceTime = 0;
unsigned long lastSelectDebounceTime = 0;
const unsigned long debounceDelay = 50; // Debounce time in milliseconds

// Button check helper function
bool checkButtonPressed(gpio_num_t pin, bool &lastState) {
    bool currentState = digitalRead(pin);
    if (currentState != lastState) {
        lastButtonDebounceTime = millis();
    }
    
    bool buttonPressed = false;
    if ((millis() - lastButtonDebounceTime) > debounceDelay) {
        if (currentState == LOW && lastState == HIGH) {
            buttonPressed = true;
        }
    }
    
    lastState = currentState;
    return buttonPressed;
}

// Check SELECT button with debouncing
bool checkSelectButtonPressed() {
    bool currentState = digitalRead(SEL_BTN);
    if (currentState != lastSelectButtonState) {
        lastSelectDebounceTime = millis();
    }
    
    bool buttonPressed = false;
    if ((millis() - lastSelectDebounceTime) > debounceDelay) {
        if (currentState == BTN_ACT && lastSelectButtonState != BTN_ACT) {
            buttonPressed = true;
        }
    }
    
    lastSelectButtonState = currentState;
    return buttonPressed;
}

// Start jamming on the current frequency
bool startLoRaJammer() {
    // Completely reset LoRa initialization state
    extern bool loraInitialized;
    loraInitialized = false;
    loraJammerPaused = false;
    delay(100); // Allow time for hardware to stabilize
    
    // Try to initialize with multiple attempts
    int attempts = 0;
    const int maxAttempts = 3;
    bool success = false;
    
    Serial.println("Initializing LoRa for jamming...");
    
    while (attempts < maxAttempts && !success) {
        // Use direct initialization to bypass soft checks
        if (initLoRaModule()) {
            success = true;
            break;
        }
        attempts++;
        delay(500); // Longer delay between attempts
        Serial.print("Attempt ");
        Serial.print(attempts);
        Serial.println(" failed, retrying...");
    }
    
    if (!success) {
        Serial.println("Failed to initialize LoRa for jamming after multiple attempts");
        return false;
    }
    
    Serial.println("LoRa initialized for jamming");
    
    // Set jamming state
    loraJammerActive = true;
    jammerStartTime = millis();
    
    // Configure for jamming with error handling
    try {
        // Apply aggressive settings for jamming
        LoRa.setTxPower(20); // Max power
        LoRa.setSpreadingFactor(7); // Fastest data rate
        LoRa.setSignalBandwidth(500000); // Wide bandwidth
        LoRa.setCodingRate4(5); // Default coding rate
        LoRa.setFrequency(jammingFrequency * 1E6); // Set frequency in Hz
        delay(50); // Give hardware time to apply settings
        
        Serial.print("Jamming on ");
        Serial.print(jammingFrequency);
        Serial.println(" MHz");
    } catch (...) {
        // If any setting fails, disable jammer and return failure
        loraJammerActive = false;
        loraInitialized = false;
        Serial.println("Exception during jammer configuration");
        return false;
    }
    
    Serial.println("LoRa jammer started successfully");
    return true;
}

// Stop all jamming activity
bool stopLoRaJammer() {
    loraJammerActive = false;
    loraJammerPaused = false;
    
    // Reset LoRa to normal state
    LoRa.setTxPower(DEFAULT_LORA_TX_POWER);
    LoRa.setSignalBandwidth(DEFAULT_LORA_BANDWIDTH);
    LoRa.setFrequency(DEFAULT_LORA_FREQUENCY * 1E6);
    
    return true;
}

// Update the jammer settings (called periodically)
void updateJammer() {
    if (!loraJammerActive || loraJammerPaused) return;
    
    // Check if LoRa is still initialized
    extern bool loraInitialized;
    if (!loraInitialized) return;
    
    // Check if jamming duration has expired (only if not set to run indefinitely)
    if (jammerDuration != 0xFFFFFFFF && millis() - jammerStartTime > jammerDuration) {
        stopLoRaJammer();
        return;
    }
    
    // Set timeout for jamming operations
    unsigned long startTime = millis();
    const unsigned long timeout = 300; // 300ms timeout for jamming operations
    
    try {
        // If sweeping, change frequency
        if (sweepFrequency) {
            sweepStep += sweepDirection;
            if (sweepStep >= (sweepMax - sweepMin) / sweepIncrement) {
                sweepDirection = -1;
            } else if (sweepStep <= 0) {
                sweepDirection = 1;
            }
            
            int currentFreq = sweepMin + (sweepStep * sweepIncrement);
            LoRa.setFrequency(currentFreq * 1E6);
        }
        
        // Check if we've exceeded timeout
        if (millis() - startTime > timeout) {
            // Operation taking too long, possibly device hanging
            extern bool loraInitialized;
            loraInitialized = false;
            loraJammerActive = false;
            return;
        }
        
        // Send jamming noise
        LoRa.beginPacket();
        // Send random data to create interference (reduced amount to prevent hanging)
        for (int i = 0; i < 16; i++) {
            LoRa.write(random(255));
            
            // Check timeout during packet sending
            if (millis() - startTime > timeout) {
                // Operation taking too long, abort
                extern bool loraInitialized;
                loraInitialized = false;
                loraJammerActive = false;
                return;
            }
        }
        
        // End packet with timeout protection
        bool success = LoRa.endPacket();
        if (!success && millis() - startTime > 200) {
            // If ending packet took too long, mark as uninitialized
            extern bool loraInitialized;
            loraInitialized = false;
            loraJammerActive = false;
        }
    } catch (...) {
        // Any exception means hardware issue
        extern bool loraInitialized;
        loraInitialized = false;
        loraJammerActive = false;
    }
}

// Main jammer loop - called from main loop
void jammerLoop() {
    if (loraJammerActive) {
        // Check for button presses to control jamming
        if (checkButtonPressed(loraPauseButtonPin, lastPauseButtonState)) {
            loraJammerPaused = !loraJammerPaused;
            
            // Update display to show paused/active status
            tft.setTextColor(loraJammerPaused ? TFT_YELLOW : TFT_GREEN, bruceConfig.bgColor);
            tft.setTextSize(1);
            tft.drawCentreString(loraJammerPaused ? "PAUSED" : "ACTIVE ", tftWidth / 2, tftHeight / 2, 4);
            delay(300); // Brief display of status
        }
        
        // Check for SELECT button to toggle jamming on/off while staying in the interface
        if (checkSelectButtonPressed()) {
            // Toggle jamming state
            if (loraJammerPaused) {
                // Resume jamming if paused
                loraJammerPaused = false;
                tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
                tft.setTextSize(1);
                tft.drawCentreString("ACTIVE", tftWidth / 2, tftHeight / 2, 4);
            } else {
                // Pause jamming if active
                loraJammerPaused = true;
                tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
                tft.setTextSize(1);
                tft.drawCentreString("PAUSED", tftWidth / 2, tftHeight / 2, 4);
            }
            delay(300); // Debounce and display status
        }
        
        if (checkButtonPressed(loraExitButtonPin, lastExitButtonState)) {
            // Exit jamming mode
            stopLoRaJammer();
            tft.setTextColor(TFT_RED, bruceConfig.bgColor);
            tft.setTextSize(1);
            tft.drawCentreString("STOPPED", tftWidth / 2, tftHeight / 2, 4);
            delay(500); // Show stopped message before returning
            return;
        }
        
        updateJammer();
    }
}

// This function is called from the wrapper in lora.cpp
void runLoRaJammer() {
    // Check if LoRa is still initialized
    extern bool loraInitialized;
    
    if (loraJammerActive) {
        // Handle case where LoRa becomes uninitialized while jamming is active
        if (!loraInitialized && !loraJammerPaused) {
            // Try to reinitialize before giving up
            if (!initLoRaOnce()) {
                // If we can't reinitialize, we have to stop jamming
                Serial.println("LoRa became disconnected while jamming - stopping jammer");
                loraJammerActive = false;
                return;
            }
            
            // Reconfigure for jamming after successful reinitialization
            try {
                LoRa.setTxPower(20);
                LoRa.setSignalBandwidth(500000);
                LoRa.setFrequency(jammingFrequency * 1E6);
            } catch (...) {
                loraJammerActive = false;
                return;
            }
        }
    
        // Try to update with error protection
        try {
            jammerLoop(); // Use jammerLoop instead of updateJammer directly
        } catch (...) {
            // If updating fails, mark as uninitialized and stop jamming
            loraInitialized = false;
            loraJammerActive = false;
        }
    }
}

// Interactive jamming mode with full UI for controlling the jammer
void interactiveJamming() {
    // Start the jammer
    if (!startLoRaJammer()) {
        showMessageDialog("Error", "Failed to start LoRa jammer", "OK", false);
        return;
    }
    
    // Clear screen and set up UI
    tft.fillScreen(bruceConfig.bgColor);
    drawMainBorderWithTitle("LoRa Jammer");
    
    // Show instructions with SELECT button functionality
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);
    tft.drawCentreString("Jamming on " + String(jammingFrequency) + " MHz", tftWidth/2, 30, 2);
    tft.drawCentreString("UP: Pause/Resume | SELECT: Toggle | DOWN: Exit", tftWidth/2, tftHeight - 30, 1);
    
    // Status indicator
    tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
    tft.drawCentreString("ACTIVE", tftWidth/2, tftHeight/2 - 15, 4);
    
    // Packet counter and run time
    unsigned long packetCount = 0;
    unsigned long lastCountUpdate = 0;
    unsigned long startTime = millis();
    unsigned long lastStatusUpdate = 0;
    unsigned long lastWatchdogCheck = 0;
    
    // Remove duration limit - run until explicitly stopped
    jammerDuration = 0xFFFFFFFF; // Set to maximum value (effectively endless)
    
    // Loop until jammer is deactivated or user exits
    while (loraJammerActive) {
        // Run the jam loop which handles button presses
        jammerLoop();
        
        // Update status display
        unsigned long currentTime = millis();
        if (currentTime - lastStatusUpdate > 500) {
            lastStatusUpdate = currentTime;
            
            // Clear status area
            tft.fillRect(tftWidth/2 - 80, tftHeight/2 - 20, 160, 40, bruceConfig.bgColor);
            
            // Update status text and color based on state
            if (loraJammerPaused) {
                tft.setTextColor(TFT_YELLOW, bruceConfig.bgColor);
                tft.drawCentreString("PAUSED", tftWidth/2, tftHeight/2 - 15, 4);
            } else {
                tft.setTextColor(TFT_GREEN, bruceConfig.bgColor);
                tft.drawCentreString("ACTIVE", tftWidth/2, tftHeight/2 - 15, 4);
            }
        }
        
        // Update packet counter and run time if active
        if (currentTime - lastCountUpdate > 1000) {
            lastCountUpdate = currentTime;
            
            if (!loraJammerPaused) {
                packetCount += 3; // Approximate count based on jamming rate
            }
            
            // Calculate runtime
            unsigned long runTime = (currentTime - startTime) / 1000; // in seconds
            unsigned long minutes = runTime / 60;
            unsigned long seconds = runTime % 60;
            
            // Display packet count and runtime
            tft.fillRect(20, tftHeight/2 + 30, tftWidth - 40, 30, bruceConfig.bgColor);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.setTextSize(1);
            
            // Format runtime as MM:SS
            char timeStr[10];
            sprintf(timeStr, "%02lu:%02lu", minutes, seconds);
            
            tft.drawCentreString("Packets: " + String(packetCount), tftWidth/2, tftHeight/2 + 30, 2);
            tft.drawCentreString("Runtime: " + String(timeStr), tftWidth/2, tftHeight/2 + 50, 2);
        }
        
        // Watchdog - check if LoRa module needs reinitialization
        if (currentTime - lastWatchdogCheck > 10000) { // Every 10 seconds
            lastWatchdogCheck = currentTime;
            
            extern bool loraInitialized;
            if (!loraInitialized && !loraJammerPaused) {
                // Try to reinitialize
                tft.setTextColor(TFT_RED, bruceConfig.bgColor);
                tft.setTextSize(1);
                tft.drawString("Reconnecting...", 20, 10, 1);
                
                if (initLoRaOnce()) {
                    // Successfully reconnected
                    tft.fillRect(20, 10, 120, 20, bruceConfig.bgColor);
                    
                    // Reconfigure for jamming
                    try {
                        LoRa.setTxPower(20);
                        LoRa.setSpreadingFactor(7);
                        LoRa.setSignalBandwidth(500000);
                        LoRa.setFrequency(jammingFrequency * 1E6);
                    } catch (...) {
                        // Ignore errors, will try again later
                    }
                } else {
                    tft.fillRect(20, 10, 120, 20, bruceConfig.bgColor);
                }
            }
        }
        
        delay(10); // Small delay to prevent locking up the UI
    }
    
    // Show exit message briefly
    tft.fillRect(tftWidth/2 - 80, tftHeight/2 - 20, 160, 40, bruceConfig.bgColor);
    tft.setTextColor(TFT_RED, bruceConfig.bgColor);
    tft.drawCentreString("STOPPED", tftWidth/2, tftHeight/2 - 15, 4);
    delay(500);
    
    // Final cleanup
    stopLoRaJammer();
}

// Check if the jammer is currently active
bool isLoRaJammerActive() {
    return loraJammerActive;
}

// Check if the jammer is currently paused
bool isLoRaJammerPaused() {
    return loraJammerPaused;
}

#endif 