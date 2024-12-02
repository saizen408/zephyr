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
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/printk.h>
#include <zephyr/task_wdt/task_wdt.h>

/* Currently the Versal can also access this LED which
* may cause issues at boot. Considering removing the Versal's access*/
#define NUM_LEDS 2
#define LED_ON 0
#define LED_OFF 1
#define LED_ERROR 0
#define LED_STATUS 1
#define STATUS_OK 0
#define STATUS_ERROR -1

/* led0 = error | led1 = status */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define WDT_NODE DT_ALIAS(watchdog0)

/*PWW DEFINES*/
#define PWM_DEV_NODE DT_ALIAS(pwmfan0)
#define DEFAULT_PERIOD_CYCLE 88
#define DEFAULT_PULSE_CYCLE 44
#define DEFAULT_PWM_PORT 0

static const struct gpio_dt_spec leds[] = {GPIO_DT_SPEC_GET(LED0_NODE, gpios),
					   GPIO_DT_SPEC_GET(LED1_NODE, gpios)};
static const struct device *const hw_wdt_dev = DEVICE_DT_GET_OR_NULL(WDT_NODE);
static const struct device *const pwm_dev = DEVICE_DT_GET_OR_NULL(PWM_DEV_NODE);

static void task_wdt_callback(int channel_id, void *user_data)
{
	/*
	 * If the issue could be resolved, call task_wdt_feed(channel_id) here
	 * to continue operation.
	 *
	 * Otherwise we can perform some cleanup and reset the device.
	 */

	/* Indicate error */
	gpio_pin_set_dt(&leds[LED_ERROR], LED_ON);
	printk("Resetting device...\n");

	sys_reboot(SYS_REBOOT_COLD);
}

static int config_leds(void) {
	int ret = 0;
	for(int i = 0; i < NUM_LEDS; i++) {
		if(!gpio_is_ready_dt(&leds[i])) {
			printf("Status LED%d not ready\n", i);
			ret = -1;
			break;
		}
		gpio_flags_t flags = i == LED_ERROR ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE;
		if(gpio_pin_configure_dt(&leds[i], flags) != 0) {
			printf("Failed to configure LED%d\n",i);
			ret = -1;
			break;
		}
	}
	return ret;
}

static int config_wdt(void) {
	int task_wdt_id = STATUS_OK;
	if (!device_is_ready(hw_wdt_dev)) {
		//todo: add logs
		printk("Hardware watchdog not ready; ignoring it.\n");
	}
	if (task_wdt_init(hw_wdt_dev) != STATUS_OK) {
		printk("Task wdt init failure\n");
		task_wdt_id = STATUS_ERROR;
	}

	/* passing NULL instead of callback to trigger system reset */
	if(task_wdt_id == STATUS_OK){
		task_wdt_id = task_wdt_add(1100U, task_wdt_callback, NULL);
		if(task_wdt_id < 0){
			printk("Failed to retreive wdt task id: %d\n", task_wdt_id);
			task_wdt_id = STATUS_ERROR;
		}
	}

	return task_wdt_id;
}


int main(void)
{
	// todo: heartbeat code needs to be refactored into its own k_thread
	printf("%s: Initializing heartbeat task\n", CONFIG_BOARD_TARGET);

	int ret;
	
	bool led_state = false;
	int beat_count = 0;
	int sleep_time = 100;

	// configure status light and wdt
	if(config_leds() != 0 ){
		return STATUS_ERROR;
	}
	
	// configure watchdog
	int task_wdt_id = config_wdt();
	if(task_wdt_id !=0 ){
		return STATUS_ERROR;
	}

	// configure pwm fan
	if(!device_is_ready(pwm_dev)) {
		printk("PWM device is not ready\n");
		return STATUS_ERROR;
	}
	if(pwm_set_cycles(pwm_dev, DEFAULT_PWM_PORT, 
		DEFAULT_PERIOD_CYCLE, DEFAULT_PULSE_CYCLE, 0)){
		printk("Failed to set period and pulse width\n");
		return STATUS_ERROR;
	}


	// main super loop begin
	while(1) {

		ret = gpio_pin_toggle_dt(&leds[LED_STATUS]);
		if(ret < 0) {
			printf("Failed to toggle gpio\n");
			return 0;
		}
		led_state = !led_state;
		beat_count++;
		
		// pause state between 'thump-thump'
		if(led_state && beat_count >= 4){
			sleep_time = 1000;
			beat_count = 0;
		} else {
			//'thump-thump' state
			sleep_time = 100;
			// printf("thump..\n");
		}
		ret = task_wdt_feed(task_wdt_id);
		if(ret != 0){
			printk("Failed to feed watch dog: %d\n", ret);
		}
		k_msleep(sleep_time);
	}
	/* shouldn't be here*/
	return STATUS_ERROR;
}