/*
 * Copyright (c) 2024 Owusu Consulting
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* Currently the Versal can also access this LED which
* may cause issues at boot. Considering removing the Versal's access*/
#define LED1_NODE DT_ALIAS(led1)


static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

int main(void)
{
	// todo: heartbeat code needs to be refactored into its own k_thread
	printf("%s: Initializing heartbeat task\n", CONFIG_BOARD_TARGET);

	int ret;
	bool led_state = false;
	int beat_count = 0;
	int sleep_time = 100;

	if(!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if(ret < 0){
		printf("Failed to configure gpio\n");
		return 0;
	}

	while(1) {

		ret = gpio_pin_toggle_dt(&led);
		if(ret < 0) {
			printf("Failed to toggle gpio\n");
			return 0;
		}
		led_state = !led_state;
		// printf("LED state: %s\n", led_state ? "ON" : "OFF");
		// printf("beat_count: %d\n", beat_count);
		beat_count++;
		
		// pause state between 'thump-thump'
		if(led_state && beat_count >= 4){
			sleep_time = 1000;
			beat_count = 0;
		} else {
			//'thump-thump' state
			sleep_time = 100;
		}
		k_msleep(sleep_time);
	}

	return 0;
}
