#include "lora.h"
#include "core/display.h"
#include "core/config.h"

// Simple test function for LoRa transmit
bool testLoRaSend() {
    if (!initLoRaOnce()) {
        Serial.println("Failed to initialize LoRa module. Check connections and configuration.");
        return false;
    }
    
    // Send a test message
    String testMsg = "M5StickCPlus2 LoRa Test";
    if (!sendLoraMessage(testMsg)) {
        Serial.println("Failed to send test message.");
        return false;
    }
    
    Serial.println("Test message sent successfully!");
    return true;
}

// Simple test function for LoRa receive
bool testLoRaReceive() {
    if (!initLoRaOnce()) {
        Serial.println("Failed to initialize LoRa module. Check connections and configuration.");
        return false;
    }
    
    Serial.println("Waiting for a LoRa message (5 seconds)...");
    unsigned long startTime = millis();
    
    while (millis() - startTime < 5000) {
        if (isLoraMessageAvailable()) {
            String message = receiveLoraMessage();
            Serial.print("Received message: ");
            Serial.println(message);
            return true;
        }
        delay(10);
    }
    
    Serial.println("No message received in the timeout period.");
    return false;
}

// Combined test function that can be called from the menu
void runLoRaTest() {
    // Clear screen
    tft.fillScreen(bruceConfig.bgColor);
    tft.setCursor(0, 0);
    tft.setTextSize(1);
    tft.setTextColor(bruceConfig.priColor);
    
    tft.println("--- LoRa Module Test ---");
    tft.println();
    
    // Display current LoRa settings
    tft.print("Module: ");
    tft.println(getLoraModuleName(static_cast<LoRaModules>(bruceConfig.loraModule)));
    
    tft.print("Frequency: ");
    tft.print(bruceConfig.loraFreq);
    tft.println(" MHz");
    
    tft.print("Bandwidth: ");
    tft.print(bruceConfig.loraBandwidth);
    tft.println(" kHz");
    
    tft.print("Spreading Factor: ");
    tft.println(bruceConfig.loraSpreadingFactor);
    
    tft.print("TX Power: ");
    tft.print(bruceConfig.loraTxPower);
    tft.println(" dBm");
    tft.println();
    
    // Force reset the loraInitialized flag to ensure a fresh initialization attempt
    extern bool loraInitialized;
    loraInitialized = false;
    delay(100);
    
    // Test LoRa initialization
    tft.println("Testing LoRa initialization...");
    if (!initLoRaOnce()) {
        // Use priColor for error (red color not available)
        tft.setTextColor(TFT_RED);
        tft.println("FAILED: Could not initialize LoRa module.");
        tft.setTextColor(bruceConfig.priColor);
        tft.println();
        tft.println("Press any button to return");
        // Wait for button press - simple implementation
        while (digitalRead(UP_BTN) != BTN_ACT && 
               digitalRead(DW_BTN) != BTN_ACT && 
               digitalRead(SEL_BTN) != BTN_ACT) {
            delay(50);
        }
        return;
    }
    // Use green for success indicator
    tft.setTextColor(TFT_GREEN);
    tft.println("SUCCESS: LoRa module initialized.");
    tft.setTextColor(bruceConfig.priColor);
    tft.println();
    
    tft.println("Press UP to test TX");
    tft.println("Press DOWN to test RX");
    tft.println("Press M5 to exit");
    
    // Wait for button press
    while (true) {
        if (digitalRead(UP_BTN) == BTN_ACT) {
            tft.println("\nTesting TX...");
            bool result = testLoRaSend();
            tft.print("TX Test: ");
            tft.println(result ? "SUCCESS" : "FAILED");
            delay(500);
        }
        
        if (digitalRead(DW_BTN) == BTN_ACT) {
            tft.println("\nTesting RX...");
            bool result = testLoRaReceive();
            tft.print("RX Test: ");
            tft.println(result ? "SUCCESS" : "FAILED");
            delay(500);
        }
        
        if (digitalRead(SEL_BTN) == BTN_ACT) {
            break;
        }
        
        delay(100);
    }
} 