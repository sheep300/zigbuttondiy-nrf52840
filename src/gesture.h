#pragma once
#include <stdint.h>
#include <stdbool.h>
enum gesture_phase { G_CAPTURE, G_CONFIRM, G_RESET, G_NORMAL };
enum gesture_event { G_NONE, G_ARMED, G_CONFIRMED, G_CANCELLED, G_TOGGLE };
struct gesture { enum gesture_phase phase; unsigned edges; int64_t deadline; };
static inline void gesture_init(struct gesture *g, int64_t now) {
    g->phase=G_CAPTURE;g->edges=0;g->deadline=now+5000;
}
static inline enum gesture_event gesture_tick(struct gesture *g,int64_t now) {
    if ((g->phase==G_CAPTURE || g->phase==G_CONFIRM) && now>=g->deadline) {
        bool confirm=g->phase==G_CONFIRM;g->phase=G_NORMAL;g->edges=0;
        return confirm?G_CANCELLED:G_NONE;
    }
    return G_NONE;
}
static inline enum gesture_event gesture_edge(struct gesture *g,int64_t now) {
    enum gesture_event timeout=gesture_tick(g,now);
    if (timeout==G_CANCELLED) return timeout; /* boundary edge is consumed */
    if (g->phase==G_NORMAL) return G_TOGGLE;
    if (g->phase==G_RESET) return G_NONE;
    if (++g->edges==(g->phase==G_CAPTURE?6:2)) {
        g->edges=0;
        if (g->phase==G_CAPTURE) {g->phase=G_CONFIRM;g->deadline=now+5000;return G_ARMED;}
        g->phase=G_RESET;return G_CONFIRMED;
    }
    return G_NONE;
}
