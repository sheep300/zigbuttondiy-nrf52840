#pragma once
#include <stdbool.h>
#include <zboss_api.h>
void pairing_init(void);
bool pairing_button(void); /* system workqueue only; true consumes this edge */
/* Zigbee context, before the default signal handler. True means local reset completed. */
bool pairing_signal(zb_zdo_app_signal_type_t signal, zb_ret_t status);
