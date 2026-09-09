"""Exercise navigation and real press timelines without ESP hardware."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class BoxMenuTests(unittest.TestCase):
    def test_navigation_and_press_timelines(self):
        source = r'''
#include "box_menu_state.h"
#include <cassert>
using namespace box_menu;
int main() {
    State s;
    assert(s.selected == 0 && !s.entered);
    s.Apply(Action::Left); assert(s.selected == 4);
    s.Apply(Action::Right); assert(s.selected == 0);
    for (int i = 0; i < 5; ++i) {
        s.Apply(Action::Enter); assert(s.entered && s.selected == i);
        s.Apply(Action::Right); assert(s.selected == i);
        s.Apply(Action::Left); assert(s.selected == i);
        s.Apply(Action::Back); assert(!s.entered && s.selected == i);
        s.Apply(Action::Back); assert(!s.entered && s.selected == i);
        s.Apply(Action::Right);
    }
    assert(s.selected == 0);
    // Settings selection, editing, clamping, and nested return.
    s.selected = 4;
    s.Apply(Action::Enter);
    assert(s.IsSettings() && s.setting == 0 && !s.editing);
    s.Apply(Action::Enter);
    assert(s.editing);
    for (int i=0; i<12; ++i) s.Apply(Action::Left);
    assert(s.brightness == 10 && s.volume == 70);
    for (int i=0; i<12; ++i) s.Apply(Action::Right);
    assert(s.brightness == 100);
    s.Apply(Action::Back);
    assert(s.IsSettings() && !s.editing);
    s.Apply(Action::Right);
    assert(s.setting == 1);
    s.Apply(Action::Enter);
    for (int i=0; i<12; ++i) s.Apply(Action::Left);
    assert(s.volume == 0 && s.brightness == 100);
    for (int i=0; i<12; ++i) s.Apply(Action::Right);
    assert(s.volume == 100);
    s.Apply(Action::Enter);
    assert(!s.editing && s.IsSettings());
    s.Apply(Action::Back);
    assert(!s.entered && s.selected == 4);
    s.Apply(Action::Enter);
    s.Apply(Action::Left); assert(s.setting == 2);
    s.Apply(Action::Enter); assert(s.wifi_setup && !s.editing);
    s.Apply(Action::Right); s.Apply(Action::Enter);
    assert(s.wifi_setup && s.setting == 2 && s.volume == 100);
    s.Apply(Action::Back); assert(!s.wifi_setup && s.IsSettings());
    s.Apply(Action::Right); assert(s.setting == 0);
    s.Apply(Action::Left); assert(s.setting == 2);
    s.Apply(Action::Back); assert(!s.entered && !s.wifi_setup);
    s.Load(-9, 999); assert(s.brightness == 10 && s.volume == 100);
    s.Load(999, -9); assert(s.brightness == 100 && s.volume == 0);
    s.Load(40, 30); assert(s.brightness == 40 && s.volume == 30);
    Key key;
    assert(key.Update(true, 0) == Press::None);
    assert(key.Update(false, 10) == Press::None); // Bounce, no click.
    assert(key.Update(false, 50) == Press::None);
    assert(key.Update(true, 100) == Press::None);
    assert(key.Update(true, 140) == Press::None);
    assert(key.Update(false, 200) == Press::None);
    assert(key.Update(false, 240) == Press::Short);
    assert(key.Update(false, 260) == Press::None);
    assert(key.Update(true, 300) == Press::None);
    assert(key.Update(true, 340) == Press::None);
    assert(key.Update(true, 1139) == Press::None);
    assert(key.Update(true, 1140) == Press::Long);
    assert(key.Update(true, 2000) == Press::None);
    assert(key.Update(false, 2100) == Press::None);
    assert(key.Update(false, 2140) == Press::None); // No short after long.
    Key wrap;
    assert(wrap.Update(true, 0xFFFFFFC0u) == Press::None);
    assert(wrap.Update(true, 0xFFFFFFE0u) == Press::None);
    assert(wrap.Update(true, 800u) == Press::Long); // Millisecond wraparound.
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = pathlib.Path(directory) / "test.cc"
            binary = pathlib.Path(directory) / "test"
            cpp.write_text(source)
            subprocess.run([
                "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "main/boards/alientek/atk-dnesp32s3-box"),
                str(cpp), "-o", str(binary),
            ], check=True)
            subprocess.run([str(binary)], check=True)
