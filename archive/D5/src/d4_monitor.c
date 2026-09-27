/* Observations only. No ZBOSS/radio control call is added here.
 * A short spinlock makes the 30..150 second window boundary consistent
 * across workqueue and ZBOSS contexts. No I/O or delay under the lock.
 */
#include <zephyr/kernel.h>
#include "d4_monitor.h"
static struct k_spinlock lock;
static bool active;
volatile uint32_t d4_rx_observations, d4_rx_true_observations;
volatile uint32_t d4_can_sleep_signals, d4_sleep_requests, d4_sleep_accepted;
volatile uint32_t d4_rx_now, d4_last_codes;
void d4_window_begin(void)
{
    k_spinlock_key_t key = k_spin_lock(&lock);
    d4_rx_observations = d4_rx_true_observations = 0;
    d4_can_sleep_signals = d4_sleep_requests = d4_sleep_accepted = 0;
    active = true;
    k_spin_unlock(&lock, key);
}
void d4_window_end(void)
{
    k_spinlock_key_t key = k_spin_lock(&lock);
    active = false;
    k_spin_unlock(&lock, key);
}
void d4_note_rx(bool rx_on)
{
    k_spinlock_key_t key = k_spin_lock(&lock);
    if (active) { d4_rx_observations++; d4_rx_true_observations += rx_on; }
    k_spin_unlock(&lock, key);
}
void d4_note_can_sleep(void)
{
    k_spinlock_key_t key = k_spin_lock(&lock);
    if (active) { d4_can_sleep_signals++; }
    k_spin_unlock(&lock, key);
}
void d4_note_sleep_result(bool accepted)
{
    k_spinlock_key_t key = k_spin_lock(&lock);
    if (active) { d4_sleep_requests++; d4_sleep_accepted += accepted; }
    k_spin_unlock(&lock, key);
}
unsigned int d4_readout(unsigned int d3_validity, bool rx_now)
{
    /* D3's completed, connected, sufficiently sampled window gates reading. */
    if (d3_validity == 5 || d3_validity == 6) { return d3_validity; }
    k_spinlock_key_t key = k_spin_lock(&lock);
    unsigned int rx = rx_now ? 2 : (d4_rx_true_observations ? 3 : 1);
    unsigned int requests = d4_sleep_requests ? 3 : (d4_can_sleep_signals ? 2 : 1);
    unsigned int outcome = !d4_sleep_requests ? 4 :
        (d4_sleep_accepted == d4_sleep_requests ? 1 : (d4_sleep_accepted ? 2 : 3));
    d4_rx_now = rx_now;
    d4_last_codes = rx | (requests << 4) | (outcome << 8);
    k_spin_unlock(&lock, key);
    return d4_last_codes;
}
