#pragma once

#include "box_menu_state.h"
#include "display/lcd_display.h"

// Board-local overlay: background chat/network updates cannot replace the menu.
class BoxMenuDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;
    void SetupUI() override;
    void Navigate(box_menu::Action action);
    ~BoxMenuDisplay() override;

private:
    box_menu::State menu_;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* header_ = nullptr;
    lv_obj_t* counter_ = nullptr;
    lv_obj_t* card_ = nullptr;
    lv_obj_t* outgoing_ = nullptr;
    lv_obj_t* navigation_ = nullptr;
    lv_obj_t* help_ = nullptr;
    lv_obj_t* dimmer_ = nullptr;
    lv_obj_t* rows_[2] = {};
    lv_obj_t* values_[2] = {};
    lv_obj_t* bars_[2] = {};
    void UpdateSettings();
    void ApplyBrightness();
    lv_obj_t* dots_[box_menu::State::kPageCount] = {};

    lv_obj_t* Label(lv_obj_t* parent, int x, int y, int width, const char* text);
    lv_obj_t* CreateCard();
    void Transition(int direction);
    static void SetX(void* object, int32_t value);
    static void SetOpacity(void* object, int32_t value);
    static void FinishTransition(lv_anim_t* animation);
    void Render();  // Caller holds the display lock.
};
