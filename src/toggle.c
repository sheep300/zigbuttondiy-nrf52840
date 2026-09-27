/* All ZBOSS calls run in the Zigbee callback. Queue/flight state is locked.
 * No new application toggle is sent on ambiguous delivery failure. */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>
#include <zboss_api.h>
#include <zboss_api_zcl.h>
#include <zigbee/zigbee_app_utils.h>
#include "toggle.h"
#include "toggle_queue.h"
static struct toggle_queue queue;
static struct k_spinlock lock;
static struct k_work_delayable pump_work, led_work;
static atomic_t callback_pending;
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static bool led_ready, busy, timed_out;
static int64_t submitted_at;
static unsigned led_edges;
static unsigned led_interval_ms = 120;
static bool led_on;
volatile uint32_t toggle_enqueued, toggle_overflow, toggle_expired;
volatile uint32_t toggle_no_network, toggle_no_buffer, toggle_schedule_errors;
volatile uint32_t toggle_submitted, toggle_confirmed, toggle_failed, toggle_timeouts;
volatile int toggle_last_status;
static void led_handler(struct k_work *work) {
    ARG_UNUSED(work);
    k_spinlock_key_t key = k_spin_lock(&lock);
    bool again = led_edges > 0;
    if (again) { led_on = !led_on; led_edges--; }
    bool on = led_on;
    unsigned interval = led_interval_ms;
    k_spin_unlock(&lock, key);
    if (led_ready) gpio_pin_set_dt(&led, on);
    if (again) k_work_reschedule(&led_work, K_MSEC(interval));
}
void toggle_status(unsigned pulses, unsigned interval_ms) {
    k_spinlock_key_t key = k_spin_lock(&lock);
    /* One short pulse for stack confirmation; three for failure/uncertainty.
     * Closely spaced events can merge feedback; counters remain separate. */
    led_on = false;
    led_edges = pulses * 2;
    led_interval_ms = interval_ms;
    k_spin_unlock(&lock, key);
    k_work_reschedule(&led_work, K_NO_WAIT);
}
static void feedback(bool success) { toggle_status(success ? 1 : 3, 120); }
static void sent(zb_bufid_t bufid) {
    zb_zcl_command_send_status_t *result = ZB_BUF_GET_PARAM(bufid, zb_zcl_command_send_status_t);
    int status = result->status;
    zb_buf_free(bufid);
    k_spinlock_key_t key = k_spin_lock(&lock);
    bool was_timeout = timed_out;
    busy = false;
    toggle_last_status = status;
    if (status == RET_OK) toggle_confirmed++; else toggle_failed++;
    k_spin_unlock(&lock, key);
    if (!was_timeout) feedback(status == RET_OK);
    k_work_reschedule(&pump_work, K_NO_WAIT);
}
static void pump_zigbee(zb_uint8_t unused, zb_uint16_t unused2) {
    ARG_UNUSED(unused); ARG_UNUSED(unused2);
    k_spinlock_key_t key = k_spin_lock(&lock);
    unsigned expired = tq_expire(&queue, k_uptime_get());
    toggle_expired += expired;
    bool eligible = queue.count && !busy;
    k_spin_unlock(&lock, key);
    if (expired) feedback(false);
    if (!eligible) goto done;
    user_input_indicate();
    if (!ZB_JOINED()) {
        key = k_spin_lock(&lock); tq_pop(&queue); toggle_no_network++; k_spin_unlock(&lock,key);
        feedback(false); goto done;
    }
    zb_bufid_t bufid = zb_buf_get_out();
    if (!bufid) { toggle_no_buffer++; goto done; }
    key = k_spin_lock(&lock);
    /* Workqueue can expire the queue while the buffer is being allocated. */
    expired = tq_expire(&queue, k_uptime_get()); toggle_expired += expired;
    eligible = queue.count != 0;
    if (eligible) { tq_pop(&queue); busy = true; timed_out = false; submitted_at = k_uptime_get(); toggle_submitted++; }
    k_spin_unlock(&lock, key);
    if (expired) feedback(false);
    if (!eligible) { zb_buf_free(bufid); goto done; }
    zb_uint8_t *ptr = zb_zcl_start_command_header(bufid,
        ZB_ZCL_CONSTRUCT_FRAME_CONTROL(ZB_ZCL_FRAME_TYPE_CLUSTER_SPECIFIC,
            ZB_ZCL_NOT_MANUFACTURER_SPECIFIC, ZB_ZCL_FRAME_DIRECTION_TO_SRV,
            ZB_ZCL_DISABLE_DEFAULT_RESPONSE), 0, ZB_ZCL_CMD_ON_OFF_TOGGLE_ID, NULL);
    zb_addr_u destination = {0};
    /* Standard unicast with AUTO security and APS ACK enabled in this SDK.
     * Callback owns the buffer on RET_OK, including asynchronous abort. */
    zb_ret_t rc = zb_zcl_finish_and_send_packet(bufid, ptr, &destination,
        ZB_APS_ADDR_MODE_16_ENDP_PRESENT, 1, 1, ZB_AF_HA_PROFILE_ID,
        ZB_ZCL_CLUSTER_ID_ON_OFF, sent);
    if (rc != RET_OK) {
        zb_buf_free(bufid);
        key = k_spin_lock(&lock); busy = false; toggle_failed++; toggle_last_status = rc; k_spin_unlock(&lock,key);
        feedback(false);
    }
done:
    atomic_clear(&callback_pending);
    k_work_reschedule(&pump_work, K_MSEC(100));
}
static void pump_handler(struct k_work *work) {
    ARG_UNUSED(work);
    int64_t now = k_uptime_get();
    k_spinlock_key_t key = k_spin_lock(&lock);
    unsigned expired = tq_expire(&queue, now); toggle_expired += expired;
    bool timeout = busy && !timed_out && now - submitted_at >= 15000;
    if (timeout) { timed_out = true; toggle_timeouts++; }
    bool pending = queue.count != 0;
    bool flight = busy && !timed_out;
    int64_t remaining = flight ? 15000 - (now - submitted_at) : 0;
    k_spin_unlock(&lock,key);
    if (expired || timeout) feedback(false);
    if (pending && atomic_cas(&callback_pending, 0, 1)) {
        if (ZB_SCHEDULE_APP_CALLBACK2(pump_zigbee,0,0) != RET_OK) {
            atomic_clear(&callback_pending); toggle_schedule_errors++;
        }
    }
    if (pending) k_work_schedule(&pump_work,K_MSEC(100));
    else if (flight) k_work_schedule(&pump_work,K_MSEC(MAX(remaining,1)));
    /* If a callback never returns, do not free its buffer or send another
     * toggle. Report uncertainty once. A late callback safely unblocks us. */
}
void toggle_enqueue(void) {
    k_spinlock_key_t key = k_spin_lock(&lock);
    bool ok = tq_push(&queue,k_uptime_get());
    if (ok) toggle_enqueued++; else toggle_overflow++;
    k_spin_unlock(&lock,key);
    if (!ok) feedback(false);
    k_work_reschedule(&pump_work,K_NO_WAIT);
}
void toggle_init(void) {
    k_work_init_delayable(&pump_work,pump_handler);
    k_work_init_delayable(&led_work,led_handler);
    led_ready = gpio_is_ready_dt(&led) && gpio_pin_configure_dt(&led,GPIO_OUTPUT_INACTIVE) == 0;
}
