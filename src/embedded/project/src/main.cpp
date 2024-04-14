#include "bluetooth.h"
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

    int rgb_state = 0;
    int previous_state = 0;

    static constexpr int32_t SLEEP_TIME_MS = 50;

    while (true)
    {
        bluetooth::bas_notify();

        const int button_state = gpio::get_button();

        // If no error reading button
        if (button_state >= 0)
        {
            // Display button state on LED
            gpio::set_led(button_state);

            // If button pressed after being released
            if (button_state && button_state != previous_state)
            {
                // TODO send Magic BLE packet

                switch (rgb_state)
                {
                case 0:
                    gpio::set_rgb_led(1, 0, 0);
                    rgb_state = 1;
                    break;
                case 1:
                    gpio::set_rgb_led(0, 1, 0);
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
