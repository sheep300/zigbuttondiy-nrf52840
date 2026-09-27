#ifndef D2_MONITOR_H
#define D2_MONITOR_H
#include <stdbool.h>
void d2_monitor_init(void);
void d2_poll_begin(void);
void d2_poll_end(void);
unsigned int d2_classify(bool joined);
#endif
