#ifndef D4_MONITOR_H
#define D4_MONITOR_H
#include <stdbool.h>
void d4_window_begin(void);
void d4_window_end(void);
void d4_note_rx(bool rx_on);
void d4_note_can_sleep(void);
void d4_note_sleep_result(bool accepted);
unsigned int d4_readout(unsigned int d3_validity, bool rx_now);
#endif
