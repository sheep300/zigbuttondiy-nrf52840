#ifndef BATTERY_CONFIG_H
#define BATTERY_CONFIG_H
/* Identity correction until paired voltage measurements are available.
 * Do not fit an offset/gain if VDDH is regulated independently of the battery.
 */
#define BATTERY_CAL_GAIN_NUM 1000
#define BATTERY_CAL_GAIN_DEN 1000
#define BATTERY_CAL_OFFSET_MV 0
#if BATTERY_CAL_GAIN_DEN <= 0 || BATTERY_CAL_GAIN_NUM <= 0
#error "Battery calibration gain must be positive"
#endif
#endif
