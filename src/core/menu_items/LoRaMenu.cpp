#include "LoRaMenu.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "modules/lora/lora.h"
#include "modules/lora/chat.h"
#include "modules/lora/jamming.h"
#include "core/mykeyboard.h"

void LoRaMenu::optionsMenu() {
    options = {
        {"Chat",           [=]() { chatroomMenu(); }},
        {"Set Nickname",   [=]() { setNickname(); }},
#ifdef LORA_JAMMER_ENABLED
        {"LoRa Jammer",    [=]() { jammerMenu(); }},
#endif
        {"Config",         [=]() { configMenu(); }},
        {"Main Menu",      [=]() { backToMenu(); }},
    };
    
    delay(200);
    String txt = "LoRa Communication";
    
    loopOptions(options, false, true, txt.c_str());
}

void LoRaMenu::configMenu() {
    options = {
        {"LoRa Module",     [=]() { setLoraModuleMenu(); }},
        {"LoRa TX (MOSI) Pin", [=]() { setLoraTxPin(); }},
        {"LoRa RX (MISO) Pin", [=]() { setLoraRxPin(); }},
        {"LoRa SCK Pin",    [=]() { setLoraSckPin(); }},
        {"LoRa CS Pin",     [=]() { setLoraCSPin(); }},
        {"LoRa RESET Pin",  [=]() { setLoraResetPin(); }},
        {"LoRa DIO0 Pin",   [=]() { setLoraDIO0Pin(); }},
        {"LoRa Frequency",  [=]() { setLoraFrequencyMenu(); }},
        {"LoRa Bandwidth",  [=]() { setLoraBandwidthMenu(); }},
        {"LoRa Spreading",  [=]() { setLoraSpreadingFactorMenu(); }},
        {"LoRa TX Power",   [=]() { setLoraTxPowerMenu(); }},
        {"Initialize LoRa", [=]() { initLoRa(); }},
        {"Test LoRa",       [=]() { testLoRa(); }},
        {"Back",            [=]() { optionsMenu(); }},
    };

    loopOptions(options, false, true, "LoRa Config");
}

void LoRaMenu::chatroomMenu() {
    if (!initLoRaOnce()) {
        // Error dialog with centered button
        tft.fillScreen(bruceConfig.bgColor);
        drawMainBorderWithTitle("LoRa Error");
        
        // Display error message
        tft.setTextColor(TFT_RED);
        tft.setTextSize(1);
        tft.drawCentreString("Failed to initialize LoRa module.", tftWidth/2, tftHeight/2 - 20, 2);
        tft.setTextColor(bruceConfig.priColor);
        tft.drawCentreString("Please check configuration", tftWidth/2, tftHeight/2 + 10, 1);
        
        // Create a properly centered button
        int btnWidth = 80;
        int btnHeight = 30;
        int btnX = (tftWidth - btnWidth)/2;
        int btnY = tftHeight - 50;
        
        // Draw button with centered text
        tft.fillRoundRect(btnX, btnY, btnWidth, btnHeight, 5, bruceConfig.priColor);
        tft.setTextColor(bruceConfig.bgColor);
        tft.drawCentreString("Back", btnX + btnWidth/2, btnY + btnHeight/2 - 4, 2);
        
        // Wait for any button press
        while (digitalRead(UP_BTN) != BTN_ACT && 
               digitalRead(DW_BTN) != BTN_ACT && 
               digitalRead(SEL_BTN) != BTN_ACT) {
            delay(50);
        }
        
        // Debounce
        delay(300);
        
        // Return to options menu
        optionsMenu();
        return;
    }
    
    startLoraChatroom();
}

void LoRaMenu::sendCustomMessage() {
    if (!initLoRaOnce()) {
        showMessageDialog("LoRa Error", "Failed to initialize LoRa module. Check configuration.", "Back", false);
        delay(1000);
        optionsMenu();
        return;
    }
    
    String message = keyboard("", 128, "Type your message (max 128 chars):");
    
    // Don't proceed with empty messages
    if (message.length() == 0) {
        optionsMenu();
        return;
    }
    
    // Get nickname with error checking
    String nickname = getNickname();
    
    // Build message with length limit to prevent overflow
    String fullMessage = nickname + ": " + message;
    if (fullMessage.length() > 128) {
        fullMessage = fullMessage.substring(0, 128);
    }
    
    // Send with proper error handling
    bool success = false;
    try {
        success = sendLoraMessage(fullMessage);
    } catch (...) {
        success = false;
    }
    
    if (success) {
        saveMessage(fullMessage);
        showMessageDialog("Message Sent", "Your message has been sent!", "OK", false);
    } else {
        showMessageDialog("Send Failed", "Failed to send message. Try again later.", "OK", false);
    }
    
    delay(1000);
    optionsMenu();
}

void LoRaMenu::viewMessages() {
    viewLoraMessages();
    optionsMenu();
}

String LoRaMenu::getNickname() {
    if (bruceConfig.loraNickname.length() == 0) {
        bruceConfig.loraNickname = "User" + String(random(1000, 9999));
        bruceConfig.saveFile();
    }
    return bruceConfig.loraNickname;
}

void LoRaMenu::setNickname() {
    setLoraNickname();
    optionsMenu();
}

void LoRaMenu::saveMessage(String& message) {
    saveLoraMessage(message, true);
}

void LoRaMenu::sendMessage(String& message) {
    sendLoraMessage(message);
}

void LoRaMenu::initLoRa() {
    // Force reset the loraInitialized flag to ensure a fresh initialization attempt
    extern bool loraInitialized;
    loraInitialized = false;
    
    // Wait a moment for any pending operations to complete
    delay(100);
    
    // Now try to initialize
    if (initLoRaOnce()) {
        // Success dialog with centered button
        tft.fillScreen(bruceConfig.bgColor);
        drawMainBorderWithTitle("LoRa Initialized");
        
        // Display success message
        tft.setTextColor(TFT_GREEN);
        tft.setTextSize(1);
        tft.drawCentreString("LoRa module initialized successfully!", tftWidth/2, tftHeight/2 - 10, 2);
        
        // Create a properly centered button
        int btnWidth = 80;
        int btnHeight = 30;
        int btnX = (tftWidth - btnWidth)/2;
        int btnY = tftHeight - 50;
        
        // Draw button with centered text
        tft.fillRoundRect(btnX, btnY, btnWidth, btnHeight, 5, bruceConfig.priColor);
        tft.setTextColor(bruceConfig.bgColor);
        tft.drawCentreString("OK", btnX + btnWidth/2, btnY + btnHeight/2 - 4, 2);
        
        // Wait for any button press
        while (digitalRead(UP_BTN) != BTN_ACT && 
               digitalRead(DW_BTN) != BTN_ACT && 
               digitalRead(SEL_BTN) != BTN_ACT) {
            delay(50);
        }
    } else {
        // Error dialog with centered button
        tft.fillScreen(bruceConfig.bgColor);
        drawMainBorderWithTitle("LoRa Error");
        
        // Display error message
        tft.setTextColor(TFT_RED);
        tft.setTextSize(1);
        tft.drawCentreString("Failed to initialize LoRa module.", tftWidth/2, tftHeight/2 - 20, 2);
        tft.setTextColor(bruceConfig.priColor);
        tft.drawCentreString("Please check configuration", tftWidth/2, tftHeight/2 + 10, 1);
        
        // Create a properly centered button
        int btnWidth = 80;
        int btnHeight = 30;
        int btnX = (tftWidth - btnWidth)/2;
        int btnY = tftHeight - 50;
        
        // Draw button with centered text
        tft.fillRoundRect(btnX, btnY, btnWidth, btnHeight, 5, bruceConfig.priColor);
        tft.setTextColor(bruceConfig.bgColor);
        tft.drawCentreString("OK", btnX + btnWidth/2, btnY + btnHeight/2 - 4, 2);
        
        // Wait for any button press
        while (digitalRead(UP_BTN) != BTN_ACT && 
               digitalRead(DW_BTN) != BTN_ACT && 
               digitalRead(SEL_BTN) != BTN_ACT) {
            delay(50);
        }
    }
    
    // Debounce
    delay(300);
    
    // Return to the config menu
    configMenu();
}

void LoRaMenu::drawIconImg() {
    if(bruceConfig.theme.lora) {
        FS* fs = nullptr;
        if(bruceConfig.theme.fs == 1) fs=&LittleFS;
        else if (bruceConfig.theme.fs == 2) fs=&SD;
        drawImg(*fs, bruceConfig.getThemeItemImg(bruceConfig.theme.paths.lora), 0, imgCenterY, true);
    }
}

void LoRaMenu::drawIcon(float scale) {
    clearIconArea();
    
    int centerX = iconCenterX;
    int centerY = iconCenterY;
    int radius = scale * 25;
    
    // Draw satellite dish
    tft.fillCircle(centerX, centerY, radius / 2, bruceConfig.priColor);
    
    // Draw antenna wave lines
    for (int i = 1; i <= 3; i++) {
        int arcRadius = (radius / 2) + (i * 10 * scale);
        tft.drawArc(centerX, centerY, arcRadius, arcRadius, 225, 315, bruceConfig.priColor, bruceConfig.bgColor);
    }
    
    // Draw antenna pole
    int poleWidth = scale * 3;
    int poleHeight = scale * 25;
    tft.fillRect(centerX - poleWidth/2, centerY, poleWidth, poleHeight, bruceConfig.priColor);
    
    // Draw antenna base
    int baseWidth = scale * 15;
    int baseHeight = scale * 4;
    tft.fillRect(centerX - baseWidth/2, centerY + poleHeight, baseWidth, baseHeight, bruceConfig.priColor);
}

void LoRaMenu::testLoRa() {
    runLoRaTest();
    configMenu();
}

#ifdef LORA_JAMMER_ENABLED
void LoRaMenu::jammerMenu() {
    // Reset the initialization flag to ensure proper setup
    extern bool loraInitialized;
    loraInitialized = false;
    
    // Attempt to initialize the LoRa module
    if (!initLoRaModule()) {
        tft.fillScreen(bruceConfig.bgColor);
        drawMainBorderWithTitle("LoRa Jammer");
        
        // Display error message
        tft.setTextColor(TFT_RED, bruceConfig.bgColor);
        tft.setTextSize(1);
        tft.drawCentreString("Failed to initialize LoRa module", tftWidth / 2, tftHeight / 2 - 10, 2);
        
        // Draw simple button for return
        tft.fillRect(0, tftHeight - 40, tftWidth, 40, bruceConfig.priColor);
        tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
        tft.drawCentreString("Back", tftWidth / 2, tftHeight - 25, 2);
        
        // Wait for button press to go back
        while (true) {
            // Use existing button press check function or direct button read
            if (digitalRead(UP_BTN) == BTN_ACT || 
                digitalRead(DW_BTN) == BTN_ACT || 
                digitalRead(SEL_BTN) == BTN_ACT) {
                delay(200); // Debounce
                break;
            }
            delay(10);
        }
        
        // Return to options menu
        return;
    }
    
    // Start interactive jamming mode with proper UI and controls
    interactiveJamming();
    
    // Make sure jamming is stopped when returning from the function
    if (isLoRaJammerActive()) {
        stopLoRaJammer();
    }
}
#endif 