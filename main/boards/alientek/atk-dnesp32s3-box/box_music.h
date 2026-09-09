#pragma once
#include <mutex>
#include "box_menu_state.h"
class BoxMenuDisplay;
class BoxMusic {
public:
    void Start(BoxMenuDisplay* display);
    void Action(box_menu::Action action);

private:
    BoxMenuDisplay* display_ = nullptr;
    std::mutex mutex_;
    int track_ = 0;
    bool playing_ = false;
    uint32_t position_ = 0;
    uint32_t generation_ = 1;
    int64_t anchor_ = 0;
    uint32_t Position() const;
    void Run();
};
