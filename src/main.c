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

nrfx_timer_t timer_inst = NRFX_TIMER_INSTANCE(0);

void nrfx_gpiote_evt_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{

}

static void pulse_count_init(uint32_t pin)
{
	uint32_t err;
	
	err = nrfx_gpiote_init(NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY);
    NRFX_ASSERT(err == NRFX_SUCCESS);

	// Enable a GPIOTE channel in IN mode. Enable pullup when using DK buttons for test. 
	nrfx_gpiote_in_config_t gpiote_cfg = NRFX_GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
	gpiote_cfg.pull = NRF_GPIO_PIN_PULLUP;
	err = nrfx_gpiote_in_init(pin, &gpiote_cfg, nrfx_gpiote_evt_handler);
	NRFX_ASSERT(err == NRFX_SUCCESS);

	nrfx_gpiote_in_event_enable(pin, true);

	// Hack to disable the event handler on this GPIOTE channel, to avoid wasting CPU cycles
	uint8_t gpiote_ch;
	nrfx_gpiote_channel_get(pin, &gpiote_ch);
	NRF_GPIOTE->INTENCLR = BIT(gpiote_ch);

	// Initialize the timer in counter mode
	nrfx_timer_config_t timer_cfg = NRFX_TIMER_DEFAULT_CONFIG;
	timer_cfg.mode = NRF_TIMER_MODE_COUNTER;
	timer_cfg.bit_width = NRF_TIMER_BIT_WIDTH_32;
	err = nrfx_timer_init(&timer_inst, &timer_cfg, NULL);
	NRFX_ASSERT(err == NRFX_SUCCESS);

	// Set up a PPI channel to connect the GPIOTE event to the timer COUNT task
	uint8_t gppi_ch;
	err = nrfx_gppi_channel_alloc(&gppi_ch);
	NRFX_ASSERT(err == NRFX_SUCCESS);

	nrfx_gppi_channel_endpoints_setup(gppi_ch,
		nrfx_gpiote_in_event_addr_get(pin),
		nrfx_timer_task_address_get(&timer_inst, NRF_TIMER_TASK_COUNT));

	nrfx_gppi_channels_enable(BIT(gppi_ch));

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

void main(void)
{
	int ret;

	LOG_INF("Pulse count sample started");

	if (!device_is_ready(led.port)) {
		return;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}

	pulse_count_init(11);

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return;
		}

		// Reset the counter, go to sleep for a second, and print the result afterwards
		pulse_count_reset();
		k_msleep(SLEEP_TIME_MS);
		LOG_INF("Pulse count: %i", pulse_count_sample());
	}
}
