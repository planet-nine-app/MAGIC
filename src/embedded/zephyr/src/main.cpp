#include "bluetooth.h"
//#include "foo.h"
#include "gpio.h"
#include "test.h"
#include "sessionless.hpp"

#include <zephyr/kernel.h>
#include <stdio.h>

int main()
{
//    if (!gpio::initialize())
    if(false)
    {
        printf("Error: Failed to init GPIO\n");
        return -1;
    }

    if (!bluetooth::initialize())
    {
        printf("Error: Failed to init Bluetooth\n");
        return -1;
    }

    int previous_state = 0;

    const struct gpio::led_value red = gpio::make_led_value(1, 0, 0);
    const struct gpio::led_value green = gpio::make_led_value(0, 1, 0);
    const struct gpio::led_value blue = gpio::make_led_value(0, 0, 1);

    static constexpr int32_t SLEEP_TIME_MS = 50;

    while (true)
/*    {
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

                Keys keys;
                if (!sessionless::generateKeys(keys))
                {
                    gpio::set_led(0);
                    continue;
                }

                const char *msg = "Here is a message";
                unsigned char signature[SIGNATURE_SIZE_BYTES];                
                if (!sessionless::sign((unsigned char *)msg, sizeof(msg), keys.privateKey, signature))
                {
                    gpio::set_led(0);
                    continue;
                }

                if (!sessionless::verifySignature(signature, keys.publicKey, 
                                                  (unsigned char *)msg, sizeof(msg)))
                {
                    gpio::set_led(0);
                    continue;
                }

                // Verified
                gpio::set_led(1);
            }
            previous_state = button_state;
        }

        k_msleep(SLEEP_TIME_MS);
    }*/

    while(true)
    {
        gpio::set_led(1);
        k_msleep(1000);
        int foo;
        foo = test::test();
        bool scanning;
	scanning = bluetooth::start_scan();
	if(!scanning) {
            gpio::display_led_values(red, red, red);
	    break;
	}
    }

    return 0;
}
