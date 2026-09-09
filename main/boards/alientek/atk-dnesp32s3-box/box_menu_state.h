#pragma once

#include <algorithm>
#include <cstdint>

namespace box_menu {
enum class Press { None, Short, Long };
enum class Action { Left, Right, Enter, Back };

// A long press fires once and never becomes a short press on release.
class Key {
public:
    Press Update(bool down, uint32_t now) {
        if (down != candidate_) {
            candidate_ = down;
            candidate_since_ = now;
        }
        if (candidate_ != pressed_ && now - candidate_since_ >= 30) {
            pressed_ = candidate_;
            if (pressed_) {
                pressed_since_ = now;
                long_sent_ = false;
            } else if (!long_sent_) {
                return Press::Short;
            }
        }
        if (pressed_ && candidate_ && !long_sent_ && now - pressed_since_ >= 800) {
            long_sent_ = true;
            return Press::Long;
        }
        return Press::None;
    }

private:
    bool candidate_ = false;
    bool pressed_ = false;
    bool long_sent_ = false;
    uint32_t candidate_since_ = 0;
    uint32_t pressed_since_ = 0;
};

struct State {
    static constexpr int kPageCount = 5;
    int selected = 0;
    bool entered = false;
    int setting = 0;
    bool editing = false;
    int brightness = 100;
    int volume = 70;
    bool IsSettings() const { return entered && selected == kPageCount - 1; }
    void Load(int light, int sound) {
        brightness = std::clamp(light, 10, 100);
        volume = std::clamp(sound, 0, 100);
    }

    void Apply(Action action) {
        if (IsSettings()) {
            if (action == Action::Back) {
                if (editing)
                    editing = false;
                else
                    entered = false;
            } else if (action == Action::Enter) {
                editing = !editing;
            } else if (!editing) {
                setting = 1 - setting;
            } else {
                const int step = action == Action::Left ? -10 : 10;
                if (setting == 0)
                    brightness = std::clamp(brightness + step, 10, 100);
                else
                    volume = std::clamp(volume + step, 0, 100);
            }
            return;
        }
        switch (action) {
            case Action::Left:
                if (!entered)
                    selected = (selected + kPageCount - 1) % kPageCount;
                break;
            case Action::Right:
                if (!entered)
                    selected = (selected + 1) % kPageCount;
                break;
            case Action::Enter:
                entered = true;
                editing = false;
                setting = 0;
                break;
            case Action::Back:
                entered = false;
                break;
        }
    }
};
}  // namespace box_menu
