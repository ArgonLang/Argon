// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <thread>
#include <mutex>

#include "ar_routine.h"
#include "runtime.h"

using namespace argon::vm;

struct OSThread {
    OSThread *next;
    OSThread **prev;

    ArRoutine *routine;

    struct VCore *current;
    struct VCore *old;

    std::thread self;
};

struct VCore {

};

/* OSThread variables */
OSThread *ost_active = nullptr; // Working OSThread
OSThread *ost_idle = nullptr;   // IDLE OSThread
unsigned int ost_max = 10000;   // Maximum OSThread allowed
unsigned int ost_count = 0;     // OSThread counter (active + idle)
std::mutex ost_lock;            // OSThread lock (active + idle)

void PushOSThread(OSThread *ost, OSThread **list) {
    ost_lock.lock();

    if (*list == nullptr) {
        ost->next = nullptr;
        ost->prev = list;
    } else {
        ost->next = (*list);

        if ((*list) != nullptr)
            (*list)->prev = &ost->next;

        ost->prev = list;
    }

    *list = ost;

    ost_lock.unlock();
}

void RemoveOSThread(OSThread *ost) {
    ost_lock.lock();

    *ost->prev = ost->next;
    if (ost->next != nullptr)
        ost->next->prev = ost->prev;

    ost_lock.unlock();
}

inline void MoveOSThreadToActive(OSThread *ost) {
    RemoveOSThread(ost);
    PushOSThread(ost, &ost_active);
}

inline void MoveOSThreadToIdle(OSThread *ost) {
    RemoveOSThread(ost);
    PushOSThread(ost, &ost_idle);
}