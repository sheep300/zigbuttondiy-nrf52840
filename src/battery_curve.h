#pragma once
#include <stdint.h>
/* Approximate resting-voltage LiPo 1S curve, not a fuel gauge. */
static inline uint8_t battery_percent_from_mv(int mv)
{
    static const struct { int mv; uint8_t percent; } curve[] = {
        {3000, 0}, {3300, 1}, {3500, 5}, {3600, 10}, {3700, 25},
        {3800, 45}, {3900, 65}, {4000, 80}, {4100, 90}, {4200, 100}
    };
    if (mv <= curve[0].mv) { return 0; }
    for (unsigned int i = 1; i < (sizeof(curve) / sizeof(curve[0])); i++) {
        if (mv < curve[i].mv) {
            return curve[i - 1].percent +
                (mv - curve[i - 1].mv) *
                (curve[i].percent - curve[i - 1].percent) /
                (curve[i].mv - curve[i - 1].mv);
        }
    }
    return 100;
}

