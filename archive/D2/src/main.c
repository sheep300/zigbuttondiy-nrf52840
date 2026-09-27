#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>
#include <ram_pwrdn.h>
#include "d2_monitor.h"

#define BUTTON_PIN 17
#define DEBOUNCE_MS 35
#define LED_MS 20
static const struct device *gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static struct gpio_callback button_cb;
static struct k_work_delayable debounce_work, led_off_work;
static int last_state;
static struct k_work_delayable status_work;
static atomic_t status_request;
static unsigned int status_edges;
static bool status_on;
static void status_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    unsigned int request = atomic_set(&status_request, 0);
    if (request) { status_edges = request * 2; status_on = false; }
    if (!status_edges) { return; }
    status_on = !status_on;
    if (gpio_pin_set_dt(&led, status_on) != 0) { return; }
    status_edges--;
    if (status_edges) {
        k_work_reschedule(&status_work, status_on ? K_MSEC(150) : K_MSEC(350));
    }
}
static void show_status(unsigned int count)
{
    atomic_set(&status_request, count);
    k_work_reschedule(&status_work, K_MSEC(700));
}

/* RAM-only diagnostics; no periodic work, logging or flash writes. */
volatile int diag_init_error;
atomic_t diag_irq_count, diag_gpio_errors, diag_scheduler_errors;

#if defined(CONFIG_DIAG_ZIGBEE)
/* Cluster support is supplied by this add-on libzboss configuration. */
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zboss_api_zcl.h>
/* On/Off diagnostic endpoint, device version 0. */
#include <zigbee/zigbee_app_utils.h>
#include <zb_nrf_platform.h>
static zb_zcl_basic_attrs_t basic;
static zb_zcl_identify_attrs_t identify;
volatile uint32_t diag_buffer_errors, diag_not_joined, diag_sleep_signals;
volatile zb_bool_t diag_rx_on_when_idle;
volatile zb_ret_t diag_last_signal_status, diag_last_buffer_result;
ZB_ZCL_DECLARE_BASIC_SERVER_ATTRIB_LIST(basic_attrs, &basic.zcl_version, &basic.power_source);
ZB_ZCL_DECLARE_IDENTIFY_SERVER_ATTRIB_LIST(identify_attrs, &identify.identify_time);
static zb_zcl_cluster_desc_t clusters[] = {
    ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_ARRAY_SIZE(basic_attrs, zb_zcl_attr_t), basic_attrs, ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
    ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_IDENTIFY, ZB_ZCL_ARRAY_SIZE(identify_attrs, zb_zcl_attr_t), identify_attrs, ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
    ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_ON_OFF, 0, NULL, ZB_ZCL_CLUSTER_CLIENT_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
};
ZB_DECLARE_SIMPLE_DESC(2, 1);
ZB_AF_SIMPLE_DESC_TYPE(2, 1) simple_desc = {
    1, ZB_AF_HA_PROFILE_ID, ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
    0, 0, 2, 1,
    { ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_CLUSTER_ID_IDENTIFY, ZB_ZCL_CLUSTER_ID_ON_OFF }
};
ZB_AF_DECLARE_ENDPOINT_DESC(ep, 1, ZB_AF_HA_PROFILE_ID, 0, NULL,
    ZB_ZCL_ARRAY_SIZE(clusters, zb_zcl_cluster_desc_t), clusters,
    (zb_af_simple_desc_1_1_t *)&simple_desc, 0, NULL, 0, NULL);
ZBOSS_DECLARE_DEVICE_CTX_1_EP(device_ctx, ep);

static void send_toggle(zb_bufid_t bufid)
{
    if (!ZB_JOINED()) {
        diag_not_joined++;
        zb_buf_free(bufid);
        return;
    }
    zb_uint16_t destination = 0x0000;
    ZB_ZCL_ON_OFF_SEND_REQ(bufid, destination, ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        1, 1, ZB_AF_HA_PROFILE_ID, ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
        ZB_ZCL_CMD_ON_OFF_TOGGLE_ID, NULL);
}
static void button_zigbee(zb_uint8_t param, zb_uint16_t unused)
{
    ARG_UNUSED(param);
    ARG_UNUSED(unused);
    show_status(d2_classify(ZB_JOINED()));
    user_input_indicate();
    diag_last_buffer_result = zb_buf_get_out_delayed(send_toggle);
    if (diag_last_buffer_result != RET_OK) {
        diag_buffer_errors++;
    }
}
void zboss_signal_handler(zb_bufid_t bufid)
{
    zb_zdo_app_signal_type_t signal = zb_get_app_signal(bufid, NULL);
    diag_rx_on_when_idle = zb_get_rx_on_when_idle();
    if (signal == ZB_COMMON_SIGNAL_CAN_SLEEP) {
        diag_sleep_signals++;
    }
    diag_last_signal_status = zigbee_default_signal_handler(bufid);
    if (bufid) {
        zb_buf_free(bufid);
    }
}
#endif

static void led_off(struct k_work *work)
{
    ARG_UNUSED(work);
    if (gpio_pin_set_dt(&led, 0) != 0) {
        atomic_inc(&diag_gpio_errors);
    }
}
static void debounce(struct k_work *work)
{
    ARG_UNUSED(work);
    int state = gpio_pin_get(gpio0, BUTTON_PIN);
    if (state < 0) {
        atomic_inc(&diag_gpio_errors);
        return;
    }
    if (state == last_state) {
        return;
    }
    last_state = state;
#if defined(CONFIG_DIAG_ZIGBEE)
    if (ZB_SCHEDULE_APP_CALLBACK2(button_zigbee, 0, 0) != RET_OK) {
        atomic_inc(&diag_scheduler_errors);
    }
#endif
}
static void button_irq(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    atomic_inc(&diag_irq_count);
    if (gpio_pin_set_dt(&led, 1) != 0) {
        atomic_inc(&diag_gpio_errors);
    }
    if (k_work_reschedule(&led_off_work, K_MSEC(LED_MS)) < 0) {
        atomic_inc(&diag_scheduler_errors);
    }
    if (k_work_reschedule(&debounce_work, K_MSEC(DEBOUNCE_MS)) < 0) {
        atomic_inc(&diag_scheduler_errors);
    }
}
static int gpio_init(void)
{
    if (!device_is_ready(gpio0) || !gpio_is_ready_dt(&led)) {
        return -ENODEV;
    }
    int err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (err) { return err; }
    err = gpio_pin_configure(gpio0, BUTTON_PIN, GPIO_INPUT | GPIO_PULL_UP);
    if (err) { return err; }
    last_state = gpio_pin_get(gpio0, BUTTON_PIN);
    if (last_state < 0) { return last_state; }
    k_work_init_delayable(&led_off_work, led_off);
    k_work_init_delayable(&debounce_work, debounce);
    gpio_init_callback(&button_cb, button_irq, BIT(BUTTON_PIN));
    err = gpio_add_callback(gpio0, &button_cb);
    if (err) { return err; }
    return gpio_pin_interrupt_configure(gpio0, BUTTON_PIN, GPIO_INT_EDGE_BOTH);
}
int main(void)
{
    k_work_init_delayable(&status_work, status_handler);
    d2_monitor_init();
    diag_init_error = gpio_init();
    if (diag_init_error) { return 0; }
#if defined(CONFIG_DIAG_ZIGBEE)
    basic.zcl_version = ZB_ZCL_VERSION;
    basic.power_source = ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;
    identify.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;
    ZB_AF_REGISTER_DEVICE_CTX(&device_ctx);
    zigbee_configure_sleepy_behavior(true);
    diag_rx_on_when_idle = zb_get_rx_on_when_idle();
#endif
    if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY)) { power_down_unused_ram(); }
#if defined(CONFIG_DIAG_ZIGBEE)
    zigbee_enable();
#endif
    /* Block main permanently. Zephyr idle performs architecture CPU sleep. */
    k_sleep(K_FOREVER);
    return 0;
}
