/*
 * Copyright (c) 2024 Owusu Consulting
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(kondor);

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

/* PWW DEFINES */
#define PWM_DEV_NODE DT_ALIAS(pwmfan0)
#define DEFAULT_PERIOD_CYCLE 88
#define DEFAULT_PULSE_CYCLE 44
#define MAX_PULSE_CYCLE DEFAULT_PERIOD_CYCLE
// https://docs.amd.com/r/en-US/ds957-versal-ai-core/Recommended-Operating-Conditions
#define SOC_TARGET_TEMP 35
#define SOC_MAX_TEMP 100
#define SOC_MIN_TEMP 0
#define DEFAULT_PWM_PORT 0

/* Shared BRAM*/
#define SHARED_BRAM_BASE_OFFSET CONFIG_SHARED_BRAM_BASE_OFFSET

static const struct gpio_dt_spec leds[] = {GPIO_DT_SPEC_GET(LED0_NODE, gpios),
					   GPIO_DT_SPEC_GET(LED1_NODE, gpios)};
static const struct device *const hw_wdt_dev = DEVICE_DT_GET_OR_NULL(WDT_NODE);
static const struct device *const pwm_dev = DEVICE_DT_GET_OR_NULL(PWM_DEV_NODE);

/*PID structures*/
typedef struct {
	float Dt;
	float Max;
	float Min;
	float Kp;
	float Kd;
	float Ki;
	float prevError;
	float Integral;
	float currDuty;
} FixedPid;

static FixedPid pid;

void pidInit(FixedPid *pid) {
	pid->Dt = 0.1;
	pid->Max = 100;
	pid->Min = 5;
	pid->Kp = 0.1;
	pid->Kd = 0.01;
	pid->Ki = 0.5;
	pid->currDuty = 50;

}

static uint32_t calcTempAdjust(FixedPid *pid, float currTemp){
	// calculate error
	float tempError = SOC_TARGET_TEMP - currTemp;

	// proportional term
	float pout = pid->Kp * tempError;

	// integral term
	float iout = pid->Ki * pid->Integral;

	// derivative term
	float derivative = (tempError - pid->prevError) / pid->Dt;
	float dout = pid->Kd * derivative;

	// calculate total output
	float output = pout + iout + dout;
	LOG_DBG("Actual output: %.3f", (double)output);

	// Restrict to max/min
	if(pid->currDuty >= pid->Max){
		output = pid->Max;
		pid->currDuty = pid->Max;
	} else if(pid->currDuty < pid->Min){
		output = pid->Min;
		pid->currDuty = pid->Min;
	} else {
		pid->currDuty += (output * -1);
	}


	// save error to previous error
	pid->prevError = tempError;
	LOG_DBG("Current temp is: %.3f | Adujust duty cycle to: %.3f ", (double)currTemp, (double)pid->currDuty);

	return (uint32_t)((pid->currDuty/(float)100.0)*(float)DEFAULT_PERIOD_CYCLE);

}

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
	LOG_ERR("Watchdog triggered. Resetting device...");

	sys_reboot(SYS_REBOOT_COLD);
}

static int config_leds(void) {
	int ret = 0;
	for(int i = 0; i < NUM_LEDS; i++) {
		if(!gpio_is_ready_dt(&leds[i])) {
			LOG_ERR("Status LED%d not ready", i);
			ret = -1;
			break;
		}
		gpio_flags_t flags = i == LED_ERROR ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE;
		if(gpio_pin_configure_dt(&leds[i], flags) != 0) {
			LOG_ERR("Failed to configure LED%d",i);
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
		LOG_WRN("Hardware watchdog not ready; ignoring it.");
	}
	if (task_wdt_init(hw_wdt_dev) != STATUS_OK) {
		LOG_ERR("Task wdt init failure");
		task_wdt_id = STATUS_ERROR;
	}

	/* passing NULL instead of callback to trigger system reset */
	if(task_wdt_id == STATUS_OK){
		task_wdt_id = task_wdt_add(1100U, task_wdt_callback, NULL);
		if(task_wdt_id < 0){
			LOG_ERR("Failed to retreive wdt task id: %d", task_wdt_id);
			task_wdt_id = STATUS_ERROR;
		}
	}

	return task_wdt_id;
}


int main(void)
{
	// todo: heartbeat code needs to be refactored into its own k_thread
	LOG_INF("%s: Initializing sys_control", CONFIG_BOARD_TARGET);

	int ret;
	
	bool led_state = false;
	int beat_count = 0;
	int sleep_time = 100;
	float currTemp = 0;
	uint32_t currDuty = MAX_PULSE_CYCLE;

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
		LOG_ERR("PWM device is not ready");
		return STATUS_ERROR;
	}

	//initialize pid for pwm control
	pidInit(&pid);
	
	// initialize fan to max speed
	if(pwm_set_cycles(pwm_dev, DEFAULT_PWM_PORT, 
		DEFAULT_PERIOD_CYCLE, DEFAULT_PULSE_CYCLE, 0)){
		LOG_ERR("Failed to set period and pulse width");
		return STATUS_ERROR;
	}


	// main super loop begin
	while(1) {

		ret = gpio_pin_toggle_dt(&leds[LED_STATUS]);
		if(ret < 0) {
			LOG_ERR("Failed to toggle gpio");
			return 0;
		}
		led_state = !led_state;
		beat_count++;
		
		// pause state between 'thump-thump'
		if(led_state && beat_count >= 4){
			sleep_time = 1000;
			beat_count = 0;
			
			// check contents of shared bram (temp value provided by versal syscon)
			currTemp = (float)sys_read32((mem_addr_t)(uintptr_t)SHARED_BRAM_BASE_OFFSET) / (float)1000.0;
			
			LOG_DBG("%x currTemp: %.3f", SHARED_BRAM_BASE_OFFSET, (double)currTemp);
			// make sure value read is between 5 and 95 C
			if(currTemp && (currTemp < (SOC_MAX_TEMP-5)) && (currTemp > (SOC_MIN_TEMP+5)) ){
				// invoke pid routine to update pwm
				currDuty = calcTempAdjust(&pid, currTemp);
				LOG_DBG("setting pwm to: %u", currDuty);
			} else {
				currDuty = MAX_PULSE_CYCLE;
				LOG_ERR("Failed to read sysMon temperature");
			}
			if(pwm_set_cycles(pwm_dev, DEFAULT_PWM_PORT, 
				DEFAULT_PERIOD_CYCLE, currDuty, 0)){
				LOG_ERR("Failed to set period and pulse width of %u", currDuty);
				return STATUS_ERROR;
			}
		} else {
			//'thump-thump' state
			sleep_time = 100;
		}
		ret = task_wdt_feed(task_wdt_id);
		if(ret != 0){
			LOG_ERR("Failed to feed watch dog: %d", ret);
		}
		k_msleep(sleep_time);
	}
	/* shouldn't be here*/
	return STATUS_ERROR;
}