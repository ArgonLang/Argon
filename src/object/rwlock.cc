// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include <thread>

#include "rwlock.h"

using namespace argon::object;

#define IDLE_TIMES  10000

#define COUNTER_WIDTH       (0x01 << ((sizeof(unsigned int) * 8) - 1))
#define COUNTER_NEG         (~COUNTER_WIDTH)

#define ISZERO(counter)     (((counter) & COUNTER_NEG) == 0)
#define SETW(counter)       ((counter) | COUNTER_WIDTH)
#define UNSETW(counter)     ((counter) & COUNTER_NEG)
#define WREQUIRED(counter)  ((((counter) & COUNTER_WIDTH)) == COUNTER_WIDTH)

void RWLock::Lock() {
    unsigned int current;
    bool ok = false;

    while (!ok) {
        current = this->cf.load(std::memory_order_consume);

        if (WREQUIRED(current)) {
            std::this_thread::yield();
            continue;
        }

        ok = this->cf.compare_exchange_weak(current, SETW(current), std::memory_order_relaxed);
    }

    short idle_times = IDLE_TIMES;
    while (!ISZERO(this->cf)) {
        if (--idle_times == 0) {
            std::this_thread::yield();
            idle_times = IDLE_TIMES;
        }
    }
}

void RWLock::RLock() {
    unsigned int current = this->cf.load(std::memory_order_consume);

    while (true) {
        if (WREQUIRED(current))
            std::this_thread::yield();

        current = this->cf.fetch_add(1);

        if (!WREQUIRED(current))
            return;

        current = this->cf.fetch_sub(1);
    }
}

void RWLock::RUnlock() {
    this->cf.fetch_sub(1);
}

void RWLock::Unlock() {
    unsigned int current = this->cf.load(std::memory_order_consume);
    unsigned int desired;

    do {
        assert(WREQUIRED(current));
        desired = UNSETW(current);
    } while (!this->cf.compare_exchange_weak(current, desired, std::memory_order_relaxed));
}
