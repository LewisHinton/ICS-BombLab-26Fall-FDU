#include "defuser.h"

#include <inttypes.h>
#include <stdio.h>

/* All arithmetic recovered from 32-bit registers is modulo 2^32. */
static int write_answer(char *out, size_t cap, const uint32_t *values, size_t count) {
    if (out == NULL || cap == 0 || count == 0) return 1;

    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        int size = snprintf(out + used, cap - used, "%s%" PRIu32,
                            i ? " " : "", values[i]);
        if (size < 0 || (size_t)size >= cap - used) return 1;
        used += (size_t)size;
    }
    return 0;
}

int solve_phase_1(uint64_t challenge, char *out, size_t cap) {
    uint32_t low = (uint32_t)challenge;
    uint32_t high = (uint32_t)(challenge >> 32);
    uint32_t middle = (uint32_t)(challenge >> 16);

    /* p1_compute: step0(low, high), step1(high, middle), step2(step1, low). */
    uint32_t first = 2U * low + (high ^ 0x8c80U);
    uint32_t second = (5U * high) ^ (middle + 0xe36dU);
    uint32_t third = 3U * (second - low) + 0x4382U;
    const uint32_t values[] = {third ^ first};
    return write_answer(out, cap, values, 1);
}

int solve_phase_2(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[6];
    values[0] = ((uint32_t)(challenge >> 12) ^ 0xfU) & 0xfU;
    for (unsigned i = 1; i < 6; ++i) {
        uint32_t term = ((uint32_t)(challenge >> (i + 17)) & 0x1fU) + 1U;
        values[i] = (3U * values[i - 1] - i) ^ term;
    }
    return write_answer(out, cap, values, 6);
}

int solve_phase_3(uint64_t challenge, char *out, size_t cap) {
    uint32_t selector = (uint32_t)challenge & 7U;
    uint32_t x = (uint32_t)(challenge >> 3) ^ (uint32_t)(challenge >> 32);
    uint32_t result;

    /* The jump table orders helpers as {1, 7, 6, 3, 2, 4, 0, 5}. */
    switch (selector) {
        case 0: result = 5U * x + 0x8bc9U; break;
        case 1: result = (x ^ 0x1ef9U) + (x >> 1); break;
        case 2: result = 5U * x + 0xaee6U; break;
        case 3: result = 5U * x + 0xf4d5U; break;
        case 4: result = (2U * x) ^ 0x3651U; break;
        case 5: result = (4U * x) ^ 0x1154U; break;
        case 6: result = (x ^ 0xea48U) + (x >> 2); break;
        default: result = 5U * x + 0x1b5f3U; break;
    }
    const uint32_t values[] = {selector, result};
    return write_answer(out, cap, values, 2);
}

static uint32_t phase4_walk(uint32_t target, uint32_t low, uint32_t high,
                            uint32_t salt) {
    uint32_t middle = low + ((high - low) >> 1);
    if (target == middle) return (middle + 0x82U) ^ salt;

    /* Each return combines the child result with this frame's midpoint. */
    if (target < middle) {
        uint32_t child = phase4_walk(target, low, middle - 1U, salt);
        return (5U * child) ^ (middle + salt) ^ 4U;
    }
    uint32_t child = phase4_walk(target, middle + 1U, high, salt);
    return (5U * child) ^ (middle + salt) ^ 7U;
}

int solve_phase_4(uint64_t challenge, char *out, size_t cap) {
    uint32_t target = (uint32_t)challenge & 0x7fU;
    uint32_t salt = (uint32_t)(challenge >> 7) ^ (uint32_t)(challenge >> 32);
    const uint32_t values[] = {phase4_walk(target, 0, 127, salt)};
    return write_answer(out, cap, values, 1);
}

int solve_phase_5(uint64_t challenge, char *out, size_t cap) {
    /* Original layout: next pointer at +0, tag at +8, value at +12.
     * Replace fixed binary addresses with indices into the same 12 nodes.
     * Index 12 denotes NULL; no permitted traversal reaches it. */
    static const struct {
        unsigned next;
        uint32_t tag;
        uint32_t value;
    } nodes[] = {
        { 1, 0x072fU, 0x6fc4U},
        {12, 0x090dU, 0x707cU},
        { 4, 0x0032U, 0x4a85U},
        {10, 0x0132U, 0xbd6eU},
        { 7, 0x024bU, 0xf43dU},
        { 8, 0x0414U, 0x848eU},
        {11, 0x0553U, 0x48acU},
        { 5, 0x0a11U, 0x68e9U},
        { 6, 0x033cU, 0x61e3U},
        { 0, 0x0629U, 0x2332U},
        { 9, 0x0b2cU, 0xbba1U},
        { 3, 0x083bU, 0x5384U}
    };
    static const unsigned heads[] = {2, 4, 7, 5, 8, 6, 11, 3};

    unsigned node = heads[challenge & 7U];
    unsigned links = (unsigned)(challenge >> 3) & 3U;
    uint32_t value = (uint32_t)(challenge >> 6) ^
                     (uint32_t)(challenge >> 32) ^ 0xe145U;
    uint32_t tag = 0;
    /* Process the head, then follow exactly 'links' next pointers. */
    for (unsigned i = 0; i <= links; ++i) {
        tag = nodes[node].tag;
        value = (5U * (value + nodes[node].value)) ^ tag;
        node = nodes[node].next;
    }
    const uint32_t values[] = {value, tag};
    return write_answer(out, cap, values, 2);
}

/* Only fixed, nonzero rotation counts (3 and 7) are used below. */
static uint32_t rotate_right(uint32_t x, unsigned count) {
    return (x >> count) | (x << (32U - count));
}

static uint32_t phase6_op0(uint32_t x) {
    return (x + 0xef61U) ^ (x >> 5);
}

static uint32_t phase6_op1(uint32_t x) {
    return ((x << 5) | (x >> 27)) ^ 0x5653U;
}

static uint32_t phase6_op2(uint32_t x) {
    return ((x << 5) + 0xe64cU) ^ (7U * x);
}

static uint32_t phase6_op3(uint32_t x) {
    return rotate_right(x, 3);
}

static uint32_t phase6_op4(uint32_t x) {
    return ((x >> 3) ^ x) + 0x22f2U;
}

static uint32_t phase6_op5(uint32_t x) {
    return (x ^ 0xbbd8U) + 8U * x;
}

int solve_phase_6(uint64_t challenge, char *out, size_t cap) {
    static uint32_t (*const ops[8])(uint32_t) = {
        phase6_op1, phase6_op2, phase6_op3, phase6_op5,
        phase6_op4, phase6_op3, phase6_op0, phase6_op4
    };
    uint32_t x = (uint32_t)(challenge >> 6) ^ (uint32_t)(challenge >> 32);
    x = ops[challenge & 7U](x) ^ 0x0d56U;
    x = ops[(challenge >> 3) & 7U](x) + 0x057aU;
    const uint32_t values[] = {x};
    return write_answer(out, cap, values, 1);
}

int solve_secret(uint64_t challenge, uint64_t context, char *out, size_t cap) {
    if (out == NULL || cap == 0) return 1;

    uint32_t selector = (uint32_t)context & 3U;
    uint32_t folded_challenge = (uint32_t)challenge ^ (uint32_t)(challenge >> 32);
    uint32_t folded_context = (uint32_t)context ^ (uint32_t)(context >> 32);
    uint32_t second = rotate_right(folded_challenge ^ folded_context, 7) ^
                      0xaa1cU ^ (selector * 0x8bffU) ^ (uint32_t)(context >> 16);
    uint32_t third = second ^ (folded_context + selector + 0x21e3U +
                               (folded_challenge >> 7));

    /* secret_gate copies into a local object: marker at +32, text at +36.
     * The little-endian marker 0x4e45504f spells OPEN. The resulting line
     * is 41..59 bytes, inside the gate's permitted 36..163-byte range. */
    int size = snprintf(out, cap,
                        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAOPEN%" PRIu32
                        " %" PRIu32 " %" PRIu32,
                        selector, second, third);
    return size < 0 || (size_t)size >= cap;
}
