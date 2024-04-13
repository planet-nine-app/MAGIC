#include "gpio.h"
#include <zephyr/drivers/gpio.h>
#include <stdio.h>

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led1_r = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led1_g = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct gpio_dt_spec led1_b = GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

namespace gpio
{
    bool configure_gpio(const struct gpio_dt_spec *spec, gpio_flags_t flags)
    {
        if (!gpio_is_ready_dt(spec))
        {
            printf("Error: GPIO pin not ready\n");
            return false;
        }

        const int ret = gpio_pin_configure_dt(spec, flags);
        if (ret < 0)
        {
            printf("Error: Failed to configure GPIO pin\n");
            return false;
        }

        return true;
    }

    bool initialize()
    {
        gpio_dt_spec leds[] = {led0, led1_r, led1_g, led1_b};
        for (auto &led : leds)
        {
            if (!configure_gpio(&led, GPIO_OUTPUT_INACTIVE))
            {
                printf("Error: Failed to configure LEDs\n");
                return false;
            }
        }

        if (!configure_gpio(&button, GPIO_INPUT))
        {
            printf("Error: Failed to configure button\n");
            return false;
        }

        return true;
    }

    int get_button()
    {
        return gpio_pin_get_dt(&button);
    }

    void set_led(const int state)
    {
        gpio_pin_set_dt(&led0, state);
    }

    void set_rgb_led(const int r, const int g, const int b)
    {
        gpio_pin_set_dt(&led1_r, r);
        gpio_pin_set_dt(&led1_g, g);
        gpio_pin_set_dt(&led1_b, b);
    }
}
