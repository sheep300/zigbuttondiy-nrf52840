/* D3: bounded, read-only radio/driver/HFXO observations.
 * No clock request, radio task, extra radio packet, ADC, or persistent timer.
 * The workqueue wakeups DO perturb CPU activity during the two-minute sample.
 * Counts estimate sampled occupancy, NOT measured current or exact duty cycle.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <hal/nrf_radio.h>
#include "nrf_802154_core.h"
#include "d3_monitor.h"
#include "d4_monitor.h"

#define SETTLE_SECONDS 30
#define WINDOW_SECONDS 120
#define MIN_SAMPLES 600U
static struct k_work_delayable sample_work;
static int64_t started, deadline;
static atomic_t complete, joined_observed;
static uint32_t prng = 0x52840U;
/* Written only in the workqueue. Read only after atomic publication. */
volatile uint32_t d3_samples, d3_radio_active, d3_driver_awake, d3_hfxo_on;
volatile uint32_t d3_not_joined_samples, d3_window_ms, d3_max_gap_ms;
volatile uint32_t d3_radio_permille, d3_driver_permille, d3_hfxo_permille;
volatile uint32_t d3_last_codes;
static int64_t previous_sample;

void d3_note_joined(bool joined)
{
    atomic_set(&joined_observed, joined);
}

static void sample_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    int64_t now = k_uptime_get();
    if (!deadline) {
        d4_window_begin();
        started = now;
        deadline = now + WINDOW_SECONDS * 1000;
        previous_sample = now;
    }
    uint32_t gap = (uint32_t)(now - previous_sample);
    if (gap > d3_max_gap_ms) { d3_max_gap_ms = gap; }
    previous_sample = now;
    if (now >= deadline) {
        d3_window_ms = (uint32_t)(now - started);
        d4_window_end();
        atomic_set(&complete, 1);
        return; /* No reschedule: monitoring ends here until the next reset. */
    }
    /* Reads only; these do not request HFCLK or change RADIO tasks/state. */
    uint32_t hardware_radio = NRF_RADIO->STATE;
    uint32_t hfstat = NRF_CLOCK->HFCLKSTAT;
    radio_state_t driver = nrf_802154_core_state_get();
    d3_samples++;
    d3_radio_active += (hardware_radio != NRF_RADIO_STATE_DISABLED);
    d3_driver_awake += (driver != RADIO_STATE_SLEEP);
    d3_hfxo_on += ((hfstat & CLOCK_HFCLKSTAT_STATE_Msk) != 0 &&
                  (hfstat & CLOCK_HFCLKSTAT_SRC_Msk) ==
                  (CLOCK_HFCLKSTAT_SRC_Xtal << CLOCK_HFCLKSTAT_SRC_Pos));
    d3_not_joined_samples += !atomic_get(&joined_observed);
    /* Vary sampling interval to reduce synchronization with fixed Zigbee polls.
     * This deterministic PRNG does not request entropy/crypto peripherals.
     */
    prng = prng * 1664525U + 1013904223U;
    k_work_reschedule(&sample_work, K_MSEC(50U + ((prng >> 16) % 100U)));
}

void d3_start(void)
{
    k_work_init_delayable(&sample_work, sample_handler);
    k_work_schedule(&sample_work, K_SECONDS(SETTLE_SECONDS));
}

static unsigned int band(uint32_t count, uint32_t total)
{
    if ((uint64_t)count * 100U < total) { return 1; }
    if ((uint64_t)count * 10U < total) { return 2; }
    if ((uint64_t)count * 10U < (uint64_t)total * 9U) { return 3; }
    return 4;
}

unsigned int d3_readout(bool joined)
{
    unsigned int codes;
    if (!joined) { codes = 6; }
    else if (!atomic_get(&complete)) { codes = 5; }
    else if (d3_samples < MIN_SAMPLES || d3_max_gap_ms > 1000U || d3_window_ms > 125000U) {
        codes = 5; /* Inadequate sampling; do not claim a valid occupancy. */
    }
    else if (d3_not_joined_samples) { codes = 6; }
    else {
        d3_radio_permille = d3_radio_active * 1000U / d3_samples;
        d3_driver_permille = d3_driver_awake * 1000U / d3_samples;
        d3_hfxo_permille = d3_hfxo_on * 1000U / d3_samples;
        codes = band(d3_radio_active, d3_samples) |
                (band(d3_driver_awake, d3_samples) << 4) |
                (band(d3_hfxo_on, d3_samples) << 8);
    }
    d3_last_codes = codes;
    return codes;
}
