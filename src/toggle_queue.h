#pragma once
#include <stdint.h>
#include <stdbool.h>
#define TOGGLE_QUEUE_CAPACITY 8
#define TOGGLE_MAX_AGE_MS 3000
struct toggle_queue { int64_t time[TOGGLE_QUEUE_CAPACITY]; unsigned head, count; };
static inline bool tq_push(struct toggle_queue *q, int64_t now) {
    if (q->count == TOGGLE_QUEUE_CAPACITY) return false;
    q->time[(q->head + q->count) % TOGGLE_QUEUE_CAPACITY] = now;
    q->count++; return true;
}
static inline void tq_pop(struct toggle_queue *q) {
    if (q->count) { q->head = (q->head + 1) % TOGGLE_QUEUE_CAPACITY; q->count--; }
}
static inline unsigned tq_expire(struct toggle_queue *q, int64_t now) {
    unsigned n = 0;
    while (q->count && now - q->time[q->head] >= TOGGLE_MAX_AGE_MS) { tq_pop(q); n++; }
    return n;
}
