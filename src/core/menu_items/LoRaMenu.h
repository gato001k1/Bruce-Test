#ifndef __LORA_MENU_H__
#define __LORA_MENU_H__

#include <MenuItemInterface.h>

class LoRaMenu : public MenuItemInterface {
public:
    LoRaMenu() : MenuItemInterface("LoRa") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    void drawIconImg();
    bool getTheme() { return bruceConfig.theme.lora; }

private:
    void configMenu(void);
    void chatroomMenu(void);
    void saveMessage(String& message);
    void sendMessage(String& message);
    void sendCustomMessage();
    void viewMessages();
    String getNickname();
    void setNickname();
#ifdef LORA_JAMMER_ENABLED
    void jammerMenu();
#endif
    void initLoRa();
    void testLoRa();
};

#endif 