#pragma once
#include <functional>
#include <string>

#include "box_menu_state.h"
#include "display/lcd_display.h"

// Board-local overlay: background chat/network updates cannot replace the menu.
class BoxMenuDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;
    void SetupUI() override;
    void OnMusicAction(std::function<void(box_menu::Action)> callback) {
        music_action_ = std::move(callback);
    }
    void SetMusicStatus(int track, bool playing, uint32_t position, uint32_t duration);
    void ShowOta(int progress, bool failed = false);
    void HideOta();
    void Navigate(box_menu::Action action);
    ~BoxMenuDisplay() override;
    void OnWifiSetup(std::function<void()> callback) { wifi_setup_callback_ = std::move(callback); }
    void SetWifiStatus(bool connected, bool configuring, const std::string& ssid,
                       const std::string& ip);
    void SetWifiInfo(const std::string& ssid, const std::string& url);

private:
    box_menu::State menu_;
    std::function<void(box_menu::Action)> music_action_;
    int music_track_ = 0;
    bool music_playing_ = false;
    uint32_t music_position_ = 0;
    uint32_t music_duration_ = 0;
    lv_obj_t* music_title_ = nullptr;
    lv_obj_t* music_artist_ = nullptr;
    lv_obj_t* music_time_ = nullptr;
    lv_obj_t* music_bar_ = nullptr;
    void UpdateMusic();
    lv_obj_t* ota_panel_ = nullptr;
    lv_obj_t* ota_progress_ = nullptr;
    lv_obj_t* ota_bar_ = nullptr;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* header_ = nullptr;
    lv_obj_t* counter_ = nullptr;
    lv_obj_t* card_ = nullptr;
    lv_obj_t* outgoing_ = nullptr;
    lv_obj_t* navigation_ = nullptr;
    lv_obj_t* help_ = nullptr;
    lv_obj_t* dimmer_ = nullptr;
    lv_obj_t* rows_[3] = {};
    lv_obj_t* values_[3] = {};
    lv_obj_t* bars_[2] = {};
    std::function<void()> wifi_setup_callback_;
    bool wifi_connected_ = false;
    bool wifi_configuring_ = false;
    std::string station_ssid_;
    std::string station_ip_;
    lv_obj_t* wifi_state_label_ = nullptr;
    lv_obj_t* station_label_ = nullptr;
    lv_obj_t* station_ip_label_ = nullptr;
    lv_obj_t* wifi_buttons_[2] = {};
    void UpdateWifiDetails();
    std::string wifi_ssid_;
    std::string wifi_url_;
    lv_obj_t* wifi_ssid_label_ = nullptr;
    lv_obj_t* wifi_url_label_ = nullptr;
    void UpdateSettings();
    void ApplyBrightness();
    lv_obj_t* dots_[box_menu::State::kPageCount] = {};

    lv_obj_t* Label(lv_obj_t* parent, int x, int y, int width, const char* text);
    lv_obj_t* CreateCard();
    void Transition(int direction);
    static void SetX(void* object, int32_t value);
    static void FinishTransition(lv_anim_t* animation);
    void Render();  // Caller holds the display lock.
};
