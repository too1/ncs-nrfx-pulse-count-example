/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include <helpers/nrfx_gppi.h>
#include <nrfx_timer.h>
#include <nrfx_gpiote.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_N_ALIAS_sw0, gpios);

nrfx_timer_t timer_inst = NRFX_TIMER_INSTANCE(0);

static void pulse_count_init(uint32_t pin)
{
	uint32_t err;
	
	// Initialize the GPIOTE library
	err = nrfx_gpiote_init(NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY);
    NRFX_ASSERT(err == NRFX_SUCCESS);

	// Allocate a GPIOTE channel to be used to generate input events
	uint8_t gpiote_ch;
	err = nrfx_gpiote_channel_alloc(&gpiote_ch);
	NRFX_ASSERT(err == NRFX_SUCCESS);

	// Set up the pin configuration with pullup enabled, in order to use a DK button
	nrfx_gpiote_input_config_t gpiote_cfg = {
		.pull = NRF_GPIO_PIN_PULLUP,
	};

	// Set the trigger configuration to use the recently allocated channel, and the HITOLO trigger setting
	nrfx_gpiote_trigger_config_t trigger_cfg = {
		.p_in_channel = &gpiote_ch,
		.trigger = NRFX_GPIOTE_TRIGGER_HITOLO,
	};

	// Configure the GPIOTE IN channel with the defined settings
	err = nrfx_gpiote_input_configure(pin, &gpiote_cfg, &trigger_cfg, NULL);
	NRFX_ASSERT(err == NRFX_SUCCESS);

	// Enable triggering on the pin in order to enable event generation. Interrupt is not required
	nrfx_gpiote_trigger_enable(pin, false);

	// Initialize the timer in counter mode
	nrfx_timer_config_t timer_cfg = {
		.mode = NRF_TIMER_MODE_COUNTER,
		.bit_width = NRF_TIMER_BIT_WIDTH_32,
		.frequency = 1000000,
	};

	err = nrfx_timer_init(&timer_inst, &timer_cfg, NULL);
	NRFX_ASSERT(err == NRFX_SUCCESS);

	// Allocate a PPI channel
	uint8_t gppi_ch;
	err = nrfx_gppi_channel_alloc(&gppi_ch);
	NRFX_ASSERT(err == NRFX_SUCCESS);

	// Configure the PPI channel to connect the GPIOTE event to the timer COUNT task
	nrfx_gppi_channel_endpoints_setup(gppi_ch,
		nrfx_gpiote_in_event_address_get(pin),
		nrfx_timer_task_address_get(&timer_inst, NRF_TIMER_TASK_COUNT));

	// Enable the PPI channel
	nrfx_gppi_channels_enable(BIT(gppi_ch));

	// Enable the timer
	nrfx_timer_enable(&timer_inst);
}

static void pulse_count_reset(void)
{
	nrfx_timer_clear(&timer_inst);
}

static uint32_t pulse_count_sample(void)
{
	nrfx_timer_capture(&timer_inst, NRF_TIMER_CC_CHANNEL0);
	return nrfx_timer_capture_get(&timer_inst, NRF_TIMER_CC_CHANNEL0);
}

int main(void)
{
	int ret;

	LOG_INF("Pulse count sample started");

	if (!device_is_ready(led.port)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	pulse_count_init(button.pin);

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}

		// Reset the counter, go to sleep for a second, and print the result afterwards
		pulse_count_reset();
		k_msleep(SLEEP_TIME_MS);
		LOG_INF("Pulse count: %i", pulse_count_sample());
	}

	return 0;
}
