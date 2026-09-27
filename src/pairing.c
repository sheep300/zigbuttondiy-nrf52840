#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zboss_api.h>
#include <zboss_api_zcl.h>
#include <zigbee/zigbee_app_utils.h>
#include "pairing.h"
#include "gesture.h"
#include "toggle.h"
/* Gesture state belongs solely to the system workqueue after initialization. */
static struct gesture gesture;
static struct k_work_delayable gesture_work, reset_work;
static struct k_work notice_work;
static atomic_t stack_ready, reset_state, notice;
static int64_t reset_deadline;
static bool joined_notice_pending;
volatile unsigned pairing_reset_requested, pairing_reset_completed, pairing_reset_errors;
/* reset_state: 0 idle, 1 requested, 2 submitted. The SDK owns completion. */
static void reset_zigbee(zb_uint8_t unused, zb_uint16_t unused2) {
    ARG_UNUSED(unused);ARG_UNUSED(unused2);
    if (!atomic_get(&stack_ready) || k_uptime_get()>=reset_deadline) return;
    if (!atomic_cas(&reset_state,1,2)) return;
    pairing_reset_requested++;
    zigbee_network_rejoin_abort();
    k_work_reschedule(&reset_work,K_SECONDS(15));
    zb_bdb_reset_via_local_action(0);
}
static void reset_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (atomic_get(&reset_state)==2) {
        if (k_uptime_get() < reset_deadline+10000) {
            k_work_reschedule(&reset_work,K_MSEC(reset_deadline+10000-k_uptime_get()));
        } else {pairing_reset_errors++;toggle_status(3,120);}
        return;
    }
    if (k_uptime_get()>=reset_deadline) {
        /* Claim only an unsubmitted request; a submitted reset is never repeated. */
        if (atomic_cas(&reset_state,1,0)) {gesture.phase=G_NORMAL;pairing_reset_errors++;toggle_status(3,120);}
        return;
    }
    if (atomic_get(&reset_state)==1) {
        (void)ZB_SCHEDULE_APP_CALLBACK2(reset_zigbee,0,0);
        k_work_reschedule(&reset_work,K_MSEC(100));
    }
}
static void gesture_handler(struct k_work *work) {
    ARG_UNUSED(work);
    enum gesture_event event=gesture_tick(&gesture,k_uptime_get());
    if (event==G_CANCELLED) toggle_status(3,120);
    else if (gesture.phase==G_NORMAL && joined_notice_pending) {joined_notice_pending=false;toggle_status(1,800);}
}
static void notice_handler(struct k_work *work) {
    ARG_UNUSED(work);
    int value=atomic_set(&notice,0);
    /* Preserve reset completion if join follows before this work executes. */
    if (value & 6) {
        gesture.phase=G_NORMAL;joined_notice_pending=false;
        toggle_status((value & 4)?3:2,(value & 4)?120:350);
    }
    if (value & 1) {
        if (gesture.phase==G_NORMAL) toggle_status(1,800);
        else joined_notice_pending=true;
    }
}
void pairing_init(void) {
    gesture_init(&gesture,k_uptime_get());
    k_work_init_delayable(&gesture_work,gesture_handler);
    k_work_init_delayable(&reset_work,reset_handler);
    k_work_init(&notice_work,notice_handler);
    k_work_schedule(&gesture_work,K_MSEC(5000));
    toggle_status(2,70); /* Application started; reset gesture window is open. */
}
bool pairing_button(void) {
    enum gesture_event event=gesture_edge(&gesture,k_uptime_get());
    if(event==G_TOGGLE) return false;
    if(event==G_ARMED) {
        joined_notice_pending=false;
        toggle_status(8,70);
        k_work_reschedule(&gesture_work,K_MSEC(5000));
    } else if(event==G_CONFIRMED) {
        reset_deadline=k_uptime_get()+5000;
        atomic_set(&reset_state,1);
        k_work_reschedule(&reset_work,K_NO_WAIT);
    } else if(event==G_CANCELLED) toggle_status(3,120);
    return true;
}
bool pairing_signal(zb_zdo_app_signal_type_t signal,zb_ret_t status) {
    if(signal==ZB_ZDO_SIGNAL_SKIP_STARTUP) atomic_set(&stack_ready,1);
    if(signal==ZB_ZDO_SIGNAL_LEAVE && atomic_get(&reset_state)==2) {
        atomic_clear(&reset_state);
        if(status==RET_OK) pairing_reset_completed++; else pairing_reset_errors++;
        atomic_or(&notice,status==RET_OK?2:4);k_work_submit(&notice_work);
        return status==RET_OK;
    }
    if ((signal==ZB_BDB_SIGNAL_DEVICE_REBOOT || signal==ZB_BDB_SIGNAL_STEERING) &&
         status==RET_OK && ZB_JOINED() && atomic_get(&reset_state)==0) {
        atomic_or(&notice,1);k_work_submit(&notice_work);
    }
    return false;
}
