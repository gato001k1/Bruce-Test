#include "lora.h"
#include "chat.h"
#include <LoRa.h>
#include "core/config.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/mykeyboard.h"
#include "globals.h"

// LoRa module globals
bool loraInitialized = false;
// loraJammerActive is defined in jamming.cpp
unsigned long loraJammerStartTime = 0;

// Initialize default LoRa pins from compile-time definitions if available
void initLoRaDefaultPins() {
#if defined(USE_LORA_VIA_SPI)
    // Check if pins have not been set or are invalid (GPIO_NUM_NC is -1)
    bool pinsNeedInitializing = 
        (bruceConfig.LORA_bus.sck == GPIO_NUM_NC || 
         bruceConfig.LORA_bus.miso == GPIO_NUM_NC || 
         bruceConfig.LORA_bus.mosi == GPIO_NUM_NC || 
         bruceConfig.LORA_bus.cs == GPIO_NUM_NC || 
         bruceConfig.loraDio0Pin == GPIO_NUM_NC || 
         bruceConfig.loraResetPin == GPIO_NUM_NC);
         
    if (pinsNeedInitializing) {
        Serial.println("Setting default LoRa pins from board definitions");
        
        // Set SPI bus pins
#if defined(LORA_SCK_PIN)
        bruceConfig.LORA_bus.sck = static_cast<gpio_num_t>(LORA_SCK_PIN);
#endif
#if defined(LORA_MISO_PIN)
        bruceConfig.LORA_bus.miso = static_cast<gpio_num_t>(LORA_MISO_PIN);
#endif
#if defined(LORA_MOSI_PIN)
        bruceConfig.LORA_bus.mosi = static_cast<gpio_num_t>(LORA_MOSI_PIN);
#endif
#if defined(LORA_CS_PIN)
        bruceConfig.LORA_bus.cs = static_cast<gpio_num_t>(LORA_CS_PIN);
#endif
#if defined(LORA_RESET_PIN)
        bruceConfig.loraResetPin = static_cast<gpio_num_t>(LORA_RESET_PIN);
#endif
#if defined(LORA_DIO0_PIN)
        bruceConfig.loraDio0Pin = static_cast<gpio_num_t>(LORA_DIO0_PIN);
#endif
        
        // Save to config file to persist across reboots
        bruceConfig.saveFile();
        Serial.println("LoRa pins initialized and saved to config");
    }
#endif
}

// Initialize the LoRa module
bool initLoRaModule() {
#if defined(USE_LORA_VIA_SPI)
    // If already initialized, don't initialize again
    if (loraInitialized) {
        return true;
    }
    
    // Initialize pins from defaults if needed
    initLoRaDefaultPins();
    
    // Check if we have valid pin configuration
    if (bruceConfig.LORA_bus.sck == GPIO_NUM_NC || 
        bruceConfig.LORA_bus.miso == GPIO_NUM_NC || 
        bruceConfig.LORA_bus.mosi == GPIO_NUM_NC || 
        bruceConfig.LORA_bus.cs == GPIO_NUM_NC) {
        Serial.println("ERROR: LoRa SPI pins not configured");
        return false;
    }
    
    if (bruceConfig.loraResetPin == GPIO_NUM_NC || bruceConfig.loraDio0Pin == GPIO_NUM_NC) {
        Serial.println("ERROR: LoRa control pins (Reset/DIO0) not configured");
        return false;
    }
    
    // Log the pin configuration
    Serial.println("LoRa pins:");
    Serial.printf("SCK: %d, MISO: %d, MOSI: %d, CS: %d, RESET: %d, DIO0: %d\n", 
                 bruceConfig.LORA_bus.sck, bruceConfig.LORA_bus.miso, 
                 bruceConfig.LORA_bus.mosi, bruceConfig.LORA_bus.cs,
                 bruceConfig.loraResetPin, bruceConfig.loraDio0Pin);
    
    // Set SPI pins based on configuration
    SPI.begin(bruceConfig.LORA_bus.sck, bruceConfig.LORA_bus.miso, bruceConfig.LORA_bus.mosi, bruceConfig.LORA_bus.cs);
    
    // Try to initialize LoRa with timeout
    unsigned long startTime = millis();
    const unsigned long timeout = 500; // 500ms timeout for initialization
    
    // Initialize module with protection against hanging
    bool initSuccess = false;
    
    // Prevent hanging on begin() call 
    // Use proper pin assignments from bruceConfig
    LoRa.setPins(bruceConfig.LORA_bus.cs, bruceConfig.loraResetPin, bruceConfig.loraDio0Pin);
    
    initSuccess = LoRa.begin(bruceConfig.loraFreq * 1E6);
    
    // If initialization failed, return false
    if (!initSuccess) {
        loraInitialized = false;
        return false;
    }
    
    // Configure LoRa parameters based on user settings
    LoRa.setTxPower(bruceConfig.loraTxPower, 1);
    LoRa.setSignalBandwidth(bruceConfig.loraBandwidth * 1E3);
    LoRa.setSpreadingFactor(bruceConfig.loraSpreadingFactor);
    // Use default coding rate (4/5) since bruceConfig.loraCodingRate doesn't exist
    LoRa.setCodingRate4(5);
    LoRa.setSyncWord(0x12);   // Set sync word for compatibility
    
    // Mark as initialized
    loraInitialized = true;
    return true;
#else
    // LoRa functionality not enabled for this board
    return false;
#endif
}

// Safe LoRa initialization that won't freeze
bool initLoRaOnce() {
    static unsigned long lastAttemptTime = 0;
    static int failedAttempts = 0;
    const unsigned long minRetryInterval = 3000; // Minimum 3 seconds between attempts
    unsigned long currentTime = millis();
    
    // Return success if already initialized
    if (loraInitialized) {
        return true;
    }
    
    // If too many recent attempts have failed, wait longer
    if (failedAttempts > 3 && currentTime - lastAttemptTime < 60000) {  // 1 minute cooldown after 3 failures
        return false;
    }
    
    // Check if enough time has passed since last attempt
    if (currentTime - lastAttemptTime < minRetryInterval) {
        return false; // Too soon for another attempt
    }
    
    // Update the last attempt time
    lastAttemptTime = currentTime;
    
    // Simple timeout protection
    unsigned long initStartTime = millis();
    const unsigned long maxInitTime = 800; // Max 800ms for initialization
    
    // Try to initialize
    bool success = false;
    
    // Use try/catch to prevent any initialization errors from freezing the system
    try {
        success = initLoRaModule();
        
        if (success) {
            loraInitialized = true;
            failedAttempts = 0; // Reset counter on success
            return true;
        } else {
            failedAttempts++;
            return false;
        }
    } 
    catch (...) {
        // If any exception, increment failure counter and return false
        failedAttempts++;
        return false;
    }
}

// Set LoRa frequency
bool setLoraFrequency(float frequency) {
    // Validate frequency range before setting
    if (frequency < 400 || frequency > 500) {
        Serial.println("Frequency out of valid range");
        return false;
    }
    
    bruceConfig.loraFreq = frequency;
    bruceConfig.saveFile();

#if defined(USE_LORA_VIA_SPI)
    if (loraInitialized) {
        try {
            LoRa.setFrequency(frequency * 1E6);
            return true;
        } catch (const std::exception& e) {
            Serial.print("Error setting frequency: ");
            Serial.println(e.what());
            return false;
        } catch (...) {
            Serial.println("Unknown error setting frequency");
            return false;
        }
    }
#endif

    return true;
}

// Set LoRa bandwidth
bool setLoraBandwidth(float bandwidth) {
    bruceConfig.loraBandwidth = bandwidth;
    bruceConfig.saveFile();

#if defined(USE_LORA_VIA_SPI)
    if (loraInitialized) {
        try {
            LoRa.setSignalBandwidth(bandwidth * 1E3);
        } catch (...) {
            return false;
        }
    }
#endif

    return true;
}

// Set LoRa spreading factor
bool setLoraSpreadingFactor(int sf) {
    bruceConfig.loraSpreadingFactor = sf;
    bruceConfig.saveFile();

#if defined(USE_LORA_VIA_SPI)
    if (loraInitialized) {
        try {
            LoRa.setSpreadingFactor(sf);
        } catch (...) {
            return false;
        }
    }
#endif

    return true;
}

// Set LoRa transmission power
bool setLoraTxPower(int power) {
    bruceConfig.loraTxPower = power;
    bruceConfig.saveFile();

#if defined(USE_LORA_VIA_SPI)
    if (loraInitialized) {
        try {
            LoRa.setTxPower(power);
        } catch (...) {
            return false;
        }
    }
#endif

    return true;
}

// Send a LoRa message with safety checks
bool sendLoraMessage(const String& message) {
#if defined(USE_LORA_VIA_SPI)
    // Quick check - don't even try if not initialized
    if (!loraInitialized) {
        return false; // Don't automatically try to initialize here
    }
    
    // Limit message size to prevent buffer overflows
    String truncatedMessage = message;
    if (truncatedMessage.length() > 128) {
        truncatedMessage = truncatedMessage.substring(0, 128);
    }
    
    // Set a timeout to prevent hanging
    unsigned long startTime = millis();
    const unsigned long timeout = 500; // 500ms maximum for a send operation
    
    // Attempt to send the message with protection
    bool success = false;
    try {
        // Begin packet
        LoRa.beginPacket();
        
        // Send data
        LoRa.print(truncatedMessage);
        
        // End packet with timeout protection
        success = LoRa.endPacket();
        
        // If failed, don't immediately mark as uninitialized
        // This allows for transient failures without forcing reconnection
        if (!success && millis() - startTime > 300) {
            // Only mark as uninitialized if the operation took unusually long
            loraInitialized = false;
        }
    }
    catch (...) {
        // Any exception means hardware issue - mark as uninitialized
        loraInitialized = false;
        return false;
    }
    
    return success;
#else
    // Dummy implementation for compilation
    return true;
#endif
}

// Check if a LoRa message is available, with safety
bool isLoraMessageAvailable() {
#if defined(USE_LORA_VIA_SPI)
    // Quick check - don't try to initialize here
    if (!loraInitialized) {
        return false;
    }
    
    bool available = false;
    try {
        // Set a timeout for this operation
        unsigned long startTime = millis();
        const unsigned long timeout = 100; // 100ms maximum for checking
        
        // Check for packets
        available = LoRa.parsePacket() > 0;
        
        // If operation took too long, could indicate hardware issues
        if (millis() - startTime > timeout) {
            loraInitialized = false;
            return false;
        }
    }
    catch (...) {
        // Any exception means hardware issue
        loraInitialized = false;
        return false;
    }
    
    return available;
#else
    // Dummy implementation
    return false;
#endif
}

// Receive a LoRa message safely
String receiveLoraMessage() {
#if defined(USE_LORA_VIA_SPI)
    // Quick safety check
    if (!loraInitialized) {
        return "";
    }
    
    String message = "";
    try {
        // Set timeout for the entire operation
        unsigned long startTime = millis();
        const unsigned long timeout = 200; // 200ms maximum for a receive operation
        
        // Check for packet
        int packetSize = LoRa.parsePacket();
        if (packetSize) {
            // Read the packet safely
            int bytesRead = 0;
            const int maxSize = 240; // Maximum safe message size
            
            // Read available data with timeout protection
            while (LoRa.available() && bytesRead < maxSize) {
                // Check for timeout
                if (millis() - startTime > timeout) {
                    loraInitialized = false;
                    break;
                }
                
                message += (char)LoRa.read();
                bytesRead++;
            }
        }
    }
    catch (...) {
        // Any exception means hardware issue
        loraInitialized = false;
        return "";
    }
    
    return message;
#else
    // Dummy implementation
    return "Test message";
#endif
}

// Get module name as string
String getLoraModuleName(LoRaModules module) {
    switch (module) {
        case LoRaModules::SX1276_MODULE: return "SX1276 (433MHz)";
        case LoRaModules::SX1278_MODULE: return "SX1278 (915MHz)";
        case LoRaModules::SX1262_MODULE: return "SX1262 (868MHz)";
        default:       return "Unknown";
    }
}

// UI function to set LoRa CS pin
void setLoraCSPin() {
    int pin = getNumericInput("Set LoRa CS PIN", String(bruceConfig.loraCsPin));
    if (pin > 0) {
        bruceConfig.loraCsPin = static_cast<gpio_num_t>(pin);
        bruceConfig.saveFile();
        loraInitialized = false; // Force re-initialization
    }
}

// UI function to set LoRa Reset pin
void setLoraResetPin() {
    int pin = getNumericInput("Set LoRa Reset PIN", String(bruceConfig.loraResetPin));
    if (pin > 0) {
        bruceConfig.loraResetPin = static_cast<gpio_num_t>(pin);
        bruceConfig.saveFile();
        loraInitialized = false; // Force re-initialization
    }
}

// UI function to set LoRa DIO0 pin
void setLoraDIO0Pin() {
    int pin = getNumericInput("Set LoRa DIO0 PIN", String(bruceConfig.loraDio0Pin));
    if (pin > 0) {
        bruceConfig.loraDio0Pin = static_cast<gpio_num_t>(pin);
        bruceConfig.saveFile();
        loraInitialized = false; // Force re-initialization
    }
}

// UI function to set LoRa TX (MOSI) pin
void setLoraTxPin() {
    int pin = getNumericInput("Set LoRa TX (MOSI) PIN", "");
    if (pin > 0) {
        BruceConfig::SPIPins currentBus = bruceConfig.LORA_bus;
        currentBus.mosi = static_cast<gpio_num_t>(pin);
        bruceConfig.LORA_bus = currentBus;
        bruceConfig.saveFile();
        loraInitialized = false; // Force re-initialization
    }
}

// UI function to set LoRa RX (MISO) pin
void setLoraRxPin() {
    int pin = getNumericInput("Set LoRa RX (MISO) PIN", "");
    if (pin > 0) {
        BruceConfig::SPIPins currentBus = bruceConfig.LORA_bus;
        currentBus.miso = static_cast<gpio_num_t>(pin);
        bruceConfig.LORA_bus = currentBus;
        bruceConfig.saveFile();
        loraInitialized = false; // Force re-initialization
    }
}

// UI function to set LoRa SCK pin
void setLoraSckPin() {
    int pin = getNumericInput("Set LoRa SCK PIN", "");
    if (pin > 0) {
        BruceConfig::SPIPins currentBus = bruceConfig.LORA_bus;
        currentBus.sck = static_cast<gpio_num_t>(pin);
        bruceConfig.LORA_bus = currentBus;
        bruceConfig.saveFile();
        loraInitialized = false; // Force re-initialization
    }
}

// UI function to set LoRa frequency
void setLoraFrequencyMenu() {
    // Prevent potential buffer overflow by limiting input
    float freq = 0;
    try {
        String currentFreq = String(bruceConfig.loraFreq, 3);
        freq = getFloatInput("Frequency (MHz)", currentFreq);
        
        // Validate frequency range
        if (freq >= 400 && freq <= 500) {
            if (setLoraFrequency(freq)) {
                Serial.print("Successfully set frequency to: ");
                Serial.println(freq);
            } else {
                Serial.println("Failed to set frequency");
            }
        } else if (freq > 0) {
            Serial.println("Frequency out of valid range");
            showMessageDialog("Invalid Range", "Frequency must be between 400-500 MHz", "OK", true);
        }
    } catch (...) {
        Serial.println("Error in setLoraFrequencyMenu");
    }
}

// UI function to set LoRa bandwidth
void setLoraBandwidthMenu() {
    float bw = getFloatInput("Bandwidth (kHz)", String(bruceConfig.loraBandwidth));
    if (bw > 0) {
        setLoraBandwidth(bw);
    }
}

// UI function to set LoRa spreading factor
void setLoraSpreadingFactorMenu() {
    int sf = getNumericInput("Spreading Factor (7-12)", String(bruceConfig.loraSpreadingFactor));
    if (sf >= 7 && sf <= 12) {
        setLoraSpreadingFactor(sf);
    }
}

// UI function to set LoRa transmission power
void setLoraTxPowerMenu() {
    int power = getNumericInput("TX Power (2-20 dBm)", String(bruceConfig.loraTxPower));
    if (power >= 2 && power <= 20) {
        setLoraTxPower(power);
    }
}

// UI function to select LoRa module type
void setLoraModuleMenu() {
    int module = bruceConfig.loraModule;
    
    options = {
        {"SX1276 (433MHz)", [&]() { module = LoRaModules::SX1276_MODULE; }},
        {"SX1278 (915MHz)", [&]() { module = LoRaModules::SX1278_MODULE; }},
        {"SX1262 (868MHz)", [&]() { module = LoRaModules::SX1262_MODULE; }}
    };
    
    loopOptions(options, module);
    
    bruceConfig.loraModule = static_cast<LoRaModules>(module);
    bruceConfig.saveFile();
    loraInitialized = false; // Force re-initialization
}

// UI function to set LoRa nickname
void setLoraNickname() {
    String nickname = keyboard(bruceConfig.loraNickname, 20, "Enter LoRa Nickname:");
    if (nickname.length() > 0) {
        bruceConfig.loraNickname = nickname;
        bruceConfig.saveFile();
        // Convert to proper C-style string format
        String successMsg = "LoRa Nickname set to: " + nickname;
        showMessageDialog("Success", successMsg.c_str(), "OK", false);
    }
}

// This function calls runLoRaJammer() from jamming.cpp
void runLoRaJammerWrapper() {
#if defined(USE_LORA_VIA_SPI) && defined(LORA_JAMMER_ENABLED)
    extern void runLoRaJammer(); // Declare the external function
    runLoRaJammer(); // Call the function from jamming.cpp
#endif
}

// UI function for jammer settings
void setLoraJammerSettings() {
#if defined(USE_LORA_VIA_SPI) && defined(LORA_JAMMER_ENABLED)
    // These functions are now in jamming.cpp
    extern void interactiveJamming();
    extern bool isLoRaJammerActive();
    extern bool stopLoRaJammer();
    
    // Use the interactive jamming function that provides UI and button controls
    interactiveJamming();
    
    // Make sure jamming is stopped when returning from the function
    if (isLoRaJammerActive()) {
        stopLoRaJammer();
    }
#else
    showMessageDialog("Not Available", "LoRa jammer is not enabled in this build.", "OK", false);
#endif
}

void loraModule() {
    std::vector<Option> loraOptions = {
        Option("LoRa Chat", startLoraChatroom),
#if defined(USE_LORA_VIA_SPI)
        Option("Set CS Pin", setLoraCSPin),
        Option("Set Reset Pin", setLoraResetPin),
        Option("Set DIO0 Pin", setLoraDIO0Pin),
        Option("Set TX (MOSI) Pin", setLoraTxPin),
        Option("Set RX (MISO) Pin", setLoraRxPin),
        Option("Set SCK Pin", setLoraSckPin),
        Option("Set Frequency", setLoraFrequencyMenu),
        Option("Set Bandwidth", setLoraBandwidthMenu),
        Option("Set Spreading Factor", setLoraSpreadingFactorMenu),
        Option("Set TX Power", setLoraTxPowerMenu),
        Option("Set Nickname", setLoraNickname),
#endif
#if defined(USE_LORA_VIA_SPI) && defined(LORA_JAMMER_ENABLED)
        Option("LoRa Jammer", setLoraJammerSettings),
#endif
        Option("Back", [](){})
    };
    
    drawSubmenu(0, loraOptions, "LoRa");
    int selectedOption = loopOptions(loraOptions);
} 