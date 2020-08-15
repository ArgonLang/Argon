// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <thread>
#include <mutex>

#include "runtime.h"

struct OSThread {
    std::thread self;

    OSThread *next;
    OSThread **prev;

    struct VCore *vcore;
};

struct VCore {
    OSThread *ost;
};

OSThread ost_active = {};       // Working threads
OSThread ost_idle = {};         // IDLE threads

unsigned int ost_count = 0;     // Threads counter (active + idle)
unsigned int ost_max = 10000;   // Maximum threads allowed

std::mutex ost_lock;            // OSThread lock (active + idle)

void PushOSThread(OSThread *thread, OSThread *list) {
    ost_lock.lock();
    list->next = thread->next;

    if (list->next != nullptr)
        list->next->prev = &list->next;

    thread->next = list;
    list->prev = &thread->next;
    ost_lock.unlock();
}

void RemoveOSThread(OSThread *thread) {
    ost_lock.lock();
    *thread->prev = thread->next;
    if (thread->next != nullptr)
        thread->next->prev = thread->prev;
    ost_lock.unlock();
}

void MoveOSThreadToIdle(OSThread *thread) {
    RemoveOSThread(thread);
    PushOSThread(thread, &ost_idle);
}

void MoveOSThreadToActive(OSThread *thread) {
    RemoveOSThread(thread);
    PushOSThread(thread, &ost_active);
}
