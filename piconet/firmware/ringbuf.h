#ifndef PICONET_RINGBUF_H
#define PICONET_RINGBUF_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Single-producer single-consumer byte ring buffer. The producer writes
// `head` and reads `tail`; the consumer writes `tail` and reads `head`.
// Capacity must be a power of two. No locks — relies on the natural
// coherence between two cores writing different fields.

typedef struct {
    uint8_t  *buf;
    uint32_t  mask;       // capacity - 1
    volatile uint32_t head;
    volatile uint32_t tail;
} ringbuf_t;

static inline void ringbuf_init(ringbuf_t *r, uint8_t *storage, uint32_t cap) {
    r->buf  = storage;
    r->mask = cap - 1;
    r->head = 0;
    r->tail = 0;
}

static inline uint32_t ringbuf_count(const ringbuf_t *r) {
    return r->head - r->tail;
}

static inline uint32_t ringbuf_capacity(const ringbuf_t *r) {
    return r->mask + 1;
}

static inline bool ringbuf_empty(const ringbuf_t *r) {
    return r->head == r->tail;
}

static inline bool ringbuf_full(const ringbuf_t *r) {
    return ringbuf_count(r) == ringbuf_capacity(r);
}

static inline uint32_t ringbuf_free(const ringbuf_t *r) {
    return ringbuf_capacity(r) - ringbuf_count(r);
}

static inline bool ringbuf_push(ringbuf_t *r, uint8_t b) {
    if (ringbuf_full(r)) return false;
    r->buf[r->head & r->mask] = b;
    __sync_synchronize();
    r->head = r->head + 1;
    return true;
}

static inline bool ringbuf_pop(ringbuf_t *r, uint8_t *out) {
    if (ringbuf_empty(r)) return false;
    *out = r->buf[r->tail & r->mask];
    __sync_synchronize();
    r->tail = r->tail + 1;
    return true;
}

#endif
