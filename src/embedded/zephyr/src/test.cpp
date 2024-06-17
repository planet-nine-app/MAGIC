#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <errno.h>
#include <zephyr/drivers/led.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>

//#define LED_PWM_NODE_ID	 DT_COMPAT_GET_ANY_STATUS_OKAY(pwm_leds)

/*const char *led_label[] = {
	DT_FOREACH_CHILD_SEP_VARGS(LED_PWM_NODE_ID, DT_PROP_OR, (,), label, NULL)
};*/

const int num_leds = 3;

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});
//static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(DT_ALIAS(ledred), gpios);
//static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(ledgreen), gpios);
//static const struct gpio_dt_spec led_blue  = GPIO_DT_SPEC_GET(DT_ALIAS(ledblue), gpios);





namespace test
{
    int test()
    {
        //const struct device *led_pwm;
	//uint8_t led;

	//led_pwm = DEVICE_DT_GET(LED_PWM_NODE_ID);
//	if (!device_is_ready(led_pwm)) {
//		LOG_ERR("Device %s is not ready", led_pwm->name);
//		return 0;
//	}

        bool foo = true;
        int led = 0;
       
        do {
	    for (led = 0; led < num_leds; led++) {
/*                gpio_pin_toggle_dt(&led_red);
		//led_on(led_pwm, led);
                k_msleep(1000);
                gpio_pin_toggle_dt(&led_green);
                k_msleep(1000);
                //led_off(led_pwm, led);
                gpio_pin_toggle_dt(&led_blue);
                k_msleep(1000);
*/
                gpio_pin_toggle_dt(&led0);
                k_msleep(1000);
                gpio_pin_toggle_dt(&led0);
                k_msleep(1000);
	    }
            foo = false;
	} while (foo);
        return 1;
    }
}


