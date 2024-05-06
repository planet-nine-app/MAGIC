#include "bluetooth.h"
//#include "foo.h"
#include "gpio.h"

#include <zephyr/kernel.h>
#include <stdio.h>

int main()
{
    if (!gpio::initialize())
    {
        printf("Error: Failed to init GPIO\n");
        return -1;
    }

    if (!bluetooth::initialize())
    {
        printf("Error: Failed to init Bluetooth\n");
        return -1;
    }


/*    if (!foo::initialize())
    {
        printf("Error: Failed to init Bluetooth\n");
        return -1;
    }*/

    gpio::set_rgb_led(0, 0, 1);
    int rgb_state = 0;
    int previous_state = 0;

    const struct gpio::led_value red = gpio::make_led_value(1, 0, 0);
    const struct gpio::led_value green = gpio::make_led_value(0, 1, 0);
    const struct gpio::led_value blue = gpio::make_led_value(0, 0, 1);

    static constexpr int32_t SLEEP_TIME_MS = 50;

    while (true)
    {
    //    bluetooth::bas_notify();

        const int button_state = gpio::get_button();

        // If no error reading button
        if (button_state >= 0)
        {
            // Display button state on LED
//            gpio::set_led(button_state);

            // If button pressed after being released
            if (button_state && button_state != previous_state)
            {

                // TODO send Magic BLE packet

                switch (rgb_state)
                {
                case 0:
                    //gpio::display_led_values(red, green, blue);
                    //gpio::set_rgb_led(1, 0, 1);
                    bluetooth::purpled();
                    rgb_state = 1;
                    break;
                case 1:
  //                  gpio::set_rgb_led(0, 1, 1);
                    bool scanning;
                    scanning = bluetooth::start_scan();
                    if(!scanning) {
                        gpio::display_led_values(red, red, red);
                        rgb_state = 0;
                        break;
                    }
                   /* scanning = foo::start_scan();
                    if(!scanning) {
                        gpio::set_rgb_led(1, 0, 0);
                        rgb_state = 0;
                        break;
                    }*/
  //                  gpio::display_led_values(green, red, green);
                    rgb_state = 2;
                    break;
                case 2:
                    gpio::set_rgb_led(0, 0, 1);
                    rgb_state = 0;
                    break;
                default:
                    break;
                }
 
            }
            previous_state = button_state;
        }

        k_msleep(SLEEP_TIME_MS);
    }

    return 0;
}
