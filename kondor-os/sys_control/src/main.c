/*
 * Copyright (c) 2024 Owusu Consulting
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/printk.h>
#include <zephyr/task_wdt/task_wdt.h>

/* Currently the Versal can also access this LED which
* may cause issues at boot. Considering removing the Versal's access*/
#define LED1_NODE DT_ALIAS(led1)
#define WDT_NODE DT_ALIAS(watchdog0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct device *const hw_wdt_dev = DEVICE_DT_GET_OR_NULL(WDT_NODE);

// static void task_wdt_callback(int channel_id, void *user_data)
// {
// 	printk("Task watchdog channel %d callback, thread: %s\n",
// 		channel_id, k_thread_name_get((k_tid_t)user_data));

// 	/*
// 	 * If the issue could be resolved, call task_wdt_feed(channel_id) here
// 	 * to continue operation.
// 	 *
// 	 * Otherwise we can perform some cleanup and reset the device.
// 	 */

// 	printk("Resetting device...\n");

// 	sys_reboot(SYS_REBOOT_COLD);
// }


int main(void)
{
	// todo: heartbeat code needs to be refactored into its own k_thread
	printf("%s: Initializing heartbeat task\n", CONFIG_BOARD_TARGET);

	int ret;
	
	bool led_state = false;
	int beat_count = 0;
	int sleep_time = 100;

	// configure status light and wdt
	if(!gpio_is_ready_dt(&led)) {
		printf("Status LED not ready\n");
		return 0;
	}
	if(gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE) != 0) {
		printf("Failed to configure gpio\n");
		return 0;
	}
	// configure watchdog
	if (!device_is_ready(hw_wdt_dev)) {
		printk("Hardware watchdog not ready; ignoring it.\n");
	}
	if (task_wdt_init(hw_wdt_dev) != 0) {
		printk("Task wdt init failure\n");
		return 0;
	}

	/* passing NULL instead of callback to trigger system reset */
	int task_wdt_id = task_wdt_add(1100U, NULL, NULL);
	printk("task_wdt_id: %d\n", task_wdt_id);
	if(task_wdt_id < 0){
		printk("Failed to retreive wdt task id: %d\n", task_wdt_id);
		return 0;
	}

	while(1) {

		ret = gpio_pin_toggle_dt(&led);
		if(ret < 0) {
			printf("Failed to toggle gpio\n");
			return 0;
		}
		led_state = !led_state;
		printf("LED state: %s\n", led_state ? "ON" : "OFF");
		printf("beat_count: %d\n", beat_count);
		beat_count++;
		
		// pause state between 'thump-thump'
		if(led_state && beat_count >= 4){
			sleep_time = 1000;
			beat_count = 0;
		} else {
			//'thump-thump' state
			sleep_time = 100;
		}
		ret = task_wdt_feed(task_wdt_id);
		if(ret != 0){
			printk("Failed to feed watch dog: %d\n", ret);
		}
		k_msleep(sleep_time);
	}

	return 0;
}

// /*
//  * This high-priority thread needs a tight timing
//  */
// void control_thread(void)
// {
// 	int task_wdt_id;
// 	int count = 0;

// 	printk("Control thread started.\n");

// 	/*
// 	 * Add a new task watchdog channel with custom callback function and
// 	 * the current thread ID as user data.
// 	 */
// 	task_wdt_id = task_wdt_add(100U, task_wdt_callback,
// 		(void *)k_current_get());

// 	while (true) {
// 		if (count == 50) {
// 			printk("Control thread getting stuck...\n");
// 			k_sleep(K_FOREVER);
// 		}

// 		task_wdt_feed(task_wdt_id);
// 		k_sleep(K_MSEC(50));
// 		count++;
// 	}
// }

// K_THREAD_DEFINE(control, 1024, control_thread, NULL, NULL, NULL, -1, 0, 1000);

