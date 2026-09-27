/* Observation only: same-thread accounting around the original k_poll.
 * Wall time inside k_poll is NOT a measurement of CPU idle or radio-off time.
 * All calls after init run in the ZBOSS thread. No timer and no extra wakeup.
 */
#include <zephyr/kernel.h>
#include "d2_monitor.h"
static int64_t window_start, poll_start;
static uint64_t poll_ticks;
static uint32_t calls;
volatile uint32_t d2_last_poll_percent, d2_last_window_seconds, d2_last_poll_calls;
volatile unsigned int d2_last_code;
void d2_monitor_init(void)
{
    window_start = k_uptime_ticks();
}
void d2_poll_begin(void)
{
    poll_start = k_uptime_ticks();
}
void d2_poll_end(void)
{
    poll_ticks += (uint64_t)(k_uptime_ticks() - poll_start);
    calls++;
}
unsigned int d2_classify(bool joined)
{
    int64_t now = k_uptime_ticks();
    uint64_t elapsed = (uint64_t)(now - window_start);
    d2_last_window_seconds = k_ticks_to_ms_floor64(elapsed) / 1000;
    d2_last_poll_calls = calls;
    d2_last_poll_percent = elapsed ? MIN(100ULL, (poll_ticks * 100ULL) / elapsed) : 0;
    unsigned int code;
    if (!joined) { code = 4; }
    else if (d2_last_window_seconds < 60) { code = 5; }
    else if (d2_last_poll_percent >= 90) { code = 1; }
    else if (d2_last_poll_percent >= 10) { code = 2; }
    else { code = 3; }
    if (d2_last_window_seconds >= 60) {
        window_start = now;
        poll_ticks = 0;
        calls = 0;
    }
    d2_last_code = code;
    return code;
}
