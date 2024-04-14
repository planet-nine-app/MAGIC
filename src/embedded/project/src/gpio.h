#pragma once

namespace gpio
{
    bool initialize();
    int get_button();
    void set_led(const int state);
    void set_rgb_led(const int r, const int g, const int b);
}
