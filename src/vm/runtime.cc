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

enum class VCoreStatus {
    IDLE,
    RUNNING
};

struct VCore {
    std::atomic<OSThread *> ost;

    ArRoutineQueue queue;   // ArRoutine queue (No lock needed ;))

    VCoreStatus status;
};

/* OSThread variables */
OSThread *ost_active = nullptr; // Working OSThread
OSThread *ost_idle = nullptr;   // IDLE OSThread
unsigned int ost_max = 10000;   // Maximum OSThread allowed
unsigned int ost_count = 0;     // OSThread counter (active + idle)
std::mutex ost_lock;            // OSThread lock (active + idle)

/* VCore variables */
VCore *vcs = nullptr;           // List of instantiated VCore
unsigned int vcs_count = 0;     // Maximum concurrent VCore

bool argon::vm::Initialize() {
    vcs_count = std::thread::hardware_concurrency();

    if (vcs_count == 0)
        vcs_count = 2;

    // Initialize list of VCore
    vcs = (VCore *) argon::memory::Alloc(sizeof(VCore) * vcs_count);

    if (vcs == nullptr)
        return false;

    for (unsigned int i = 0; i < vcs_count; i++) {
        vcs[i].ost = nullptr;
        vcs[i].queue = ArRoutineQueue();
        vcs[i].status = VCoreStatus::IDLE;
    }

    return true;
}

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