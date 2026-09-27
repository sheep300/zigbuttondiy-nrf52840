#ifndef D3_MONITOR_H
#define D3_MONITOR_H
#include <stdbool.h>
void d3_start(void);
void d3_note_joined(bool joined);
unsigned int d3_readout(bool joined);
#endif
