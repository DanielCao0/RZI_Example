/* SPDX-License-Identifier: Apache-2.0 */
/* RZI C API example: OTAA and a confirmed four-byte counter every 5 s. */
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <rzi/lorawan.h>

LOG_MODULE_REGISTER(rzi_app, LOG_LEVEL_INF);
#define PERIOD_MS 5000
#define USER_NODE DT_PATH(zephyr_user)
#define REGION_NAME(name) DT_CAT(RZI_LORAWAN_REGION_, name)
static const struct rzi_lorawan_config config = {
	.region = REGION_NAME(DT_STRING_UNQUOTED(USER_NODE, user_lorawan_region)),
	.dev_eui = DT_PROP(USER_NODE, user_lorawan_device_eui),
	.join_eui = DT_PROP(USER_NODE, user_lorawan_join_eui),
	.network_key = DT_PROP(USER_NODE, user_lorawan_app_key),
	.application_key = DT_PROP(USER_NODE, user_lorawan_gen_app_key),
	/* Preserve this example's explicit development-only join policy. */
	.join_backoff_bypass = true,
};
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static uint32_t counter;

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
	if (!device_is_ready(dev)) { return; }
	while (!dtr && k_uptime_get() < deadline) {
		(void)uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		k_sleep(K_MSEC(100));
	}
}
static void send_counter(void)
{
	uint8_t data[4] = {counter >> 24, counter >> 16, counter >> 8, counter};
	int rc = rzi_lorawan_send(1, data, sizeof(data), true);
	if (rc) { LOG_WRN("uplink not accepted: %d", rc); return; }
	gpio_pin_set_dt(&blue, 1);
	(void)k_work_reschedule(&blue_off_work, K_MSEC(80));
	LOG_INF("confirmed uplink #%u on port 1", counter++);
}
int main(void)
{
	if (!gpio_is_ready_dt(&blue) || !gpio_is_ready_dt(&green)) { return -ENODEV; }
	int rc = gpio_pin_configure_dt(&blue, GPIO_OUTPUT_INACTIVE);
	if (rc) { return rc; }
	rc = gpio_pin_configure_dt(&green, GPIO_OUTPUT_INACTIVE);
	if (rc) { return rc; }
	joining_leds();
	wait_usb_console();
	LOG_INF("RZI C API example on %s", CONFIG_BOARD_TARGET);
	rc = rzi_lorawan_init(&config);
	if (rc) { LOG_ERR("RZI init failed: %d", rc); return rc; }
	bool ready = false;
	bool joined = false;
	int64_t next_action = 0;
	for (;;) {
		struct rzi_lorawan_event event;
		int32_t wait_ms = ready ? (int32_t)CLAMP(next_action - k_uptime_get(),
						      0, PERIOD_MS) : -1;
		rc = rzi_lorawan_get_event(&event, wait_ms);
		if (rc && rc != -EAGAIN) {
			LOG_ERR("event queue failure: %d; reboot required", rc);
			return rc;
		}
		if (!rc) {
			switch (event.type) {
			case RZI_LORAWAN_READY:
				ready = true; joined = false; next_action = 0;
				joining_leds();
				break;
			case RZI_LORAWAN_JOINED:
				LOG_INF("JOINED");
				joined = true; next_action = 0;
				joined_leds();
				break;
			case RZI_LORAWAN_JOIN_FAILED:
				LOG_WRN("JOINFAIL, retry in 5 s");
				joined = false;
				joining_leds();
				rc = rzi_lorawan_leave();
				if (rc) { LOG_WRN("leave failed: %d", rc); }
				next_action = k_uptime_get() + PERIOD_MS;
				break;
			case RZI_LORAWAN_TX_DONE:
				LOG_INF("TXDONE %s", event.tx_result == RZI_LORAWAN_TX_ACKED ?
					"ACK" : event.tx_result == RZI_LORAWAN_TX_SENT ?
					"sent, no ACK" : "not sent");
				blue_off();
				break;
			case RZI_LORAWAN_DOWNLINK:
				LOG_INF("DOWNLINK port %u, %u bytes, RSSI %d dBm, SNR %d/4 dB",
					event.downlink.port, event.downlink.size,
					event.downlink.rssi_dbm, event.downlink.snr_quarter_db);
				break;
			case RZI_LORAWAN_ERROR:
				LOG_ERR("modem error: %d; reboot required", event.error);
				return event.error;
			}
		}
		if (ready && k_uptime_get() >= next_action) {
			if (joined) { send_counter(); }
			else {
				rc = rzi_lorawan_join();
				if (rc && rc != -EBUSY) { LOG_WRN("join not accepted: %d", rc); }
			}
			next_action = k_uptime_get() + PERIOD_MS;
		}
	}
}
