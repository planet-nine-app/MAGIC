#pragma once

namespace gpio
{

    struct led_value
    {
        int r;
        int g; 
        int b;
    };

    led_value make_led_value(const int r, const int g, const int b);

    bool initialize();
    int get_button();
    void display_led_value(led_value);
    void display_led_values(const led_value first, const led_value second, const led_value third);
    void set_led(const int state);
    void set_rgb_led(const int r, const int g, const int b);
}
