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

// Prototypes
OSThread *AllocOST();

void StartOST(OSThread *);

/* OSThread variables */
OSThread *ost_active = nullptr;             // Working OSThread
OSThread *ost_idle = nullptr;               // IDLE OSThread
thread_local OSThread *ost_local = nullptr; //
unsigned int ost_max = 10000;               // Maximum OSThread allowed
unsigned int ost_count = 0;                 // OSThread counter
unsigned int ost_idle_count = 0;            // OSThread counter (idle)
std::mutex ost_lock;                        // OSThread lock (active + idle)

/* VCore variables */
VCore *vcs = nullptr;                       // List of instantiated VCore
unsigned int vcs_count = 0;                 // Maximum concurrent VCore

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
}

inline void RemoveOSThread(OSThread *ost) {
    if (ost->prev != nullptr)
        *ost->prev = ost->next;
    if (ost->next != nullptr)
        ost->next->prev = ost->prev;
}

inline void FromActiveToIdle(OSThread *ost) {
    ost_lock.lock();
    RemoveOSThread(ost);
    PushOSThread(ost, &ost_idle);
    ost_idle_count++;
    ost_lock.unlock();
}

inline void FromIdleToActive(OSThread *ost) {
    ost_lock.lock();
    RemoveOSThread(ost);
    PushOSThread(ost, &ost_active);
    ost_idle_count--;
    ost_lock.unlock();
}

bool WireVC() {
    for (unsigned int i = 0; i < vcs_count; i++) {
        auto vc = vcs + i;
        OSThread *current = vc->ost.load(std::memory_order_consume);
        if (current == nullptr) {
            if (vc->ost.compare_exchange_weak(current, ost_local, std::memory_order_relaxed)) {
                ost_local->current = vc;
                return true;
            }
        }
    }
    return false;
}

void Schedule(OSThread *self) {
    ost_local = self;

    // Find a free VCore and associate with it
    while (!WireVC());
    self->current->status = VCoreStatus::RUNNING;
    FromIdleToActive(self);

    // DO Things....
}

OSThread *AllocOST() {
    auto ost = (OSThread *) argon::memory::Alloc(sizeof(OSThread));

    if (ost != nullptr) {
        ost->next = nullptr;
        ost->prev = nullptr;

        ost->routine = nullptr;

        ost->current = nullptr;
        ost->old = nullptr;

        ost->self = std::thread();
    }

    return ost;
}

void StartOST(OSThread *ost) {
    ost_lock.lock();
    PushOSThread(ost, &ost_idle);
    ost_count++;
    ost_idle_count++;
    ost_lock.unlock();
    ost->self = std::thread(Schedule, ost);
}