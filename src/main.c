/* SPDX-License-Identifier: Apache-2.0 */
/* RZI C API example: OTAA and a confirmed four-byte counter every 5 s. */
#include <errno.h>
#include <rzi/lorawan.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(rzi_app, LOG_LEVEL_INF);
#define PERIOD_MS         5000
#define USER_NODE         DT_PATH(zephyr_user)
#define REGION_NAME(name) DT_CAT(RZI_LORAWAN_REGION_, name)
#define EVENT_READY       BIT(0)
#define EVENT_JOINED      BIT(1)
#define EVENT_JOIN_FAILED BIT(2)
#define EVENT_ERROR       BIT(3)
static const struct rzi_lorawan_join_config join_config = {
	.activation = RZI_LORAWAN_ACTIVATION_OTAA,
	.otaa =
		{
			.dev_eui = DT_PROP(USER_NODE, user_lorawan_device_eui),
			.join_eui = DT_PROP(USER_NODE, user_lorawan_join_eui),
			.network_key = DT_PROP(USER_NODE, user_lorawan_app_key),
			.application_key = DT_PROP(USER_NODE, user_lorawan_gen_app_key),
		},
};
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static uint32_t counter;
static atomic_t callback_error;
static atomic_t join_in_progress;
static rzi_lorawan_callback_handle_t callback_handle;
K_MSGQ_DEFINE(lorawan_events, sizeof(uint8_t), 8, 1);

static void green_blink(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	gpio_pin_toggle_dt(&green);
}
static void blue_off_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	gpio_pin_set_dt(&blue, 0);
}
K_TIMER_DEFINE(join_led_timer, green_blink, NULL);
K_WORK_DELAYABLE_DEFINE(blue_off_work, blue_off_handler);

static void blue_off(void)
{
	(void)k_work_cancel_delayable(&blue_off_work);
	gpio_pin_set_dt(&blue, 0);
}
static void joining_leds(void)
{
	blue_off();
	gpio_pin_set_dt(&green, 1);
	k_timer_start(&join_led_timer, K_MSEC(500), K_MSEC(500));
}
static void joined_leds(void)
{
	k_timer_stop(&join_led_timer);
	gpio_pin_set_dt(&green, 1);
	blue_off();
}
static void wait_usb_console(void)
{
	const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;
	int64_t deadline = k_uptime_get() + 10000;
	if (!device_is_ready(dev)) {
		return;
	}
	while (!dtr && k_uptime_get() < deadline) {
		(void)uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		k_sleep(K_MSEC(100));
	}
}
static void send_counter(void)
{
	uint8_t data[4] = {counter >> 24, counter >> 16, counter >> 8, counter};
	int rc = rzi_lorawan_send(1, data, sizeof(data), RZI_LORAWAN_MSG_CONFIRMED);
	if (rc) {
		LOG_WRN("uplink not accepted: %d", rc);
		return;
	}
	gpio_pin_set_dt(&blue, 1);
	(void)k_work_reschedule(&blue_off_work, K_MSEC(80));
	LOG_INF("confirmed uplink #%u on port 1", counter++);
}

static void on_state_changed(enum rzi_lorawan_state state, void *user_data)
{
	const uint8_t event = EVENT_READY;

	ARG_UNUSED(user_data);
	if (state == RZI_LORAWAN_STATE_READY && !atomic_get(&join_in_progress)) {
		(void)k_msgq_put(&lorawan_events, &event, K_NO_WAIT);
	}
}

static void on_join_done(int status, void *user_data)
{
	const uint8_t event = status == 0 ? EVENT_JOINED : EVENT_JOIN_FAILED;

	ARG_UNUSED(user_data);
	atomic_clear(&join_in_progress);
	(void)k_msgq_put(&lorawan_events, &event, K_NO_WAIT);
}

static void on_send_done(const struct rzi_lorawan_tx_result *result, void *user_data)
{
	ARG_UNUSED(user_data);
	LOG_INF("TXDONE %s", result->status == RZI_LORAWAN_TX_ACKED  ? "ACK"
			     : result->status == RZI_LORAWAN_TX_SENT ? "sent, no ACK"
								     : "not sent");
	blue_off();
}

static void on_downlink(const struct rzi_lorawan_downlink *downlink, void *user_data)
{
	ARG_UNUSED(user_data);
	LOG_INF("DOWNLINK port %u, %u bytes, RSSI %d dBm, SNR %d/4 dB", downlink->port,
		(unsigned int)downlink->size, downlink->rssi_dbm, downlink->snr_quarter_db);
}

static void on_error(int error, void *user_data)
{
	const uint8_t event = EVENT_ERROR;

	ARG_UNUSED(user_data);
	atomic_set(&callback_error, error);
	(void)k_msgq_put(&lorawan_events, &event, K_NO_WAIT);
}

static const struct rzi_lorawan_callbacks callbacks = {
	.join_done = on_join_done,
	.send_done = on_send_done,
	.downlink = on_downlink,
	.state_changed = on_state_changed,
	.error = on_error,
};

int main(void)
{
	if (!gpio_is_ready_dt(&blue) || !gpio_is_ready_dt(&green)) {
		return -ENODEV;
	}
	int rc = gpio_pin_configure_dt(&blue, GPIO_OUTPUT_INACTIVE);
	if (rc) {
		return rc;
	}
	rc = gpio_pin_configure_dt(&green, GPIO_OUTPUT_INACTIVE);
	if (rc) {
		return rc;
	}
	joining_leds();
	wait_usb_console();
	LOG_INF("RZI C API example on %s", CONFIG_BOARD_TARGET);
	rc = rzi_lorawan_register_callbacks(&callbacks, &callback_handle);
	if (!rc) {
		rc = rzi_lorawan_set_region(
			REGION_NAME(DT_STRING_UNQUOTED(USER_NODE, user_lorawan_region)));
	}
	if (!rc) {
		rc = rzi_lorawan_set_join_backoff_bypass(true);
	}
	if (!rc) {
		rc = rzi_lorawan_start();
	}
	if (rc) {
		LOG_ERR("RZI start failed: %d", rc);
		return rc;
	}
	bool ready = false;
	bool joined = false;
	int64_t next_action = 0;
	for (;;) {
		uint8_t events = 0;
		int32_t wait_ms =
			ready ? (int32_t)CLAMP(next_action - k_uptime_get(), 0, PERIOD_MS) : -1;

		(void)k_msgq_get(&lorawan_events, &events,
				 wait_ms < 0 ? K_FOREVER : K_MSEC(wait_ms));
		if (events & EVENT_READY) {
			ready = true;
			joined = false;
			next_action = 0;
			joining_leds();
		}
		if (events & EVENT_JOINED) {
			LOG_INF("JOINED");
			joined = true;
			next_action = 0;
			joined_leds();
		}
		if (events & EVENT_JOIN_FAILED) {
			LOG_WRN("JOINFAIL, retry in 5 s");
			joined = false;
			joining_leds();
			next_action = k_uptime_get() + PERIOD_MS;
		}
		if (events & EVENT_ERROR) {
			int error = (int)atomic_get(&callback_error);

			LOG_ERR("modem error: %d; reboot required", error);
			return error;
		}
		if (ready && k_uptime_get() >= next_action) {
			if (joined) {
				send_counter();
			} else {
				atomic_set(&join_in_progress, 1);
				rc = rzi_lorawan_join(&join_config);
				if (rc) {
					atomic_clear(&join_in_progress);
					if (rc != -EBUSY) {
						LOG_WRN("join not accepted: %d", rc);
					}
				}
			}
			next_action = k_uptime_get() + PERIOD_MS;
		}
	}
}
