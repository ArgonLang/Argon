// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <thread>
#include <mutex>
#include <condition_variable>

#include <object/arobject.h>

#include "areval.h"
#include "routine_queue.h"
#include "runtime.h"

using namespace argon::vm;

struct OSThread {
    OSThread *next;
    OSThread **prev;

    ArRoutine *routine;

    struct VCore *current;
    struct VCore *old;

    bool spinning;

    std::thread self;
};

enum class VCoreStatus {
    IDLE,
    RUNNING
};

struct VCore {
    std::atomic<OSThread *> ost;

    ArRoutineQueue queue;   // ArRoutine queue (No lock needed... In the future, I hope ...))

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
bool should_stop = false;                   // If true all thread should be stopped
std::atomic_uint ost_spinning_count = 0;    // OSThread in spinning
std::mutex ost_lock;                        // OSThread lock (active + idle)
std::mutex ost_cond_lock;                   // OSThread mutex for condition variable
std::condition_variable_any ost_cond;       //

/* VCore variables */
VCore *vcs = nullptr;                       // List of instantiated VCore
unsigned int vcs_count = 0;                 // Maximum concurrent VCore
std::atomic_uint vcs_idle_count = 0;        // IDLE VCore

/* ArRoutine Global queue */
ArRoutineQueue routine_gq;
// std::mutex gqr_lock;

thread_local ArRoutine *routine_main = nullptr; // Main routine, this routine is created by calling Context::Eval
thread_local argon::object::ArObject *last_error = nullptr;

bool argon::vm::Initialize() {
    vcs_count = std::thread::hardware_concurrency();

    if (vcs_count == 0)
        vcs_count = 2;

    vcs_idle_count = vcs_count;

    // Initialize memory subsystem
    argon::memory::InitializeMemory();

    // Initialize list of VCore
    vcs = (VCore *) argon::memory::Alloc(sizeof(VCore) * vcs_count);

    if (vcs == nullptr)
        return false;

    for (unsigned int i = 0; i < vcs_count; i++) {
        vcs[i].ost = nullptr;
        new(&((vcs + i)->queue))ArRoutineQueue(ARGON_VM_QUEUE_MAX_ROUTINES);
        vcs[i].status = VCoreStatus::IDLE;
    }

    return true;
}

bool argon::vm::Shutdown() {
    short attempt = 10;

    should_stop = true;

    while (ost_count > 0 && attempt > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        attempt--;
    }

    if (ost_count == 0) {
        argon::memory::FinalizeMemory();
        return true;
    }

    return false;
}

ArRoutine *argon::vm::GetRoutine() {
    if (ost_local != nullptr) {
        // We are into a spawned thread
        // If a new context was created from a native call into spawned thread,
        // returns routine_main instead ost_local->routine
        if (routine_main != nullptr)
            return routine_main;
        return ost_local->routine;
    }

    return routine_main;
}

Context *argon::vm::GetContext() {
    ArRoutine *routine = GetRoutine();

    if (routine != nullptr)
        return routine->context;

    return nullptr;
}

bool argon::vm::IsPanicking() {
    auto routine = GetRoutine();

    if (routine == nullptr)
        return last_error != nullptr;

    return routine->panic != nullptr;
}

argon::object::ArObject *argon::vm::GetLastError() {
    argon::object::ArObject *err = nullptr;
    auto routine = GetRoutine();

    if (routine == nullptr) {
        err = last_error;
        last_error = nullptr;
        return err;
    }

    if (routine->panic != nullptr) {
        // First, try to get error from active defer (if any)
        if ((err = RoutineRecover(routine)) == nullptr) {
            err = routine->panic->object;
            argon::object::IncRef(err);
            RoutinePopPanic(routine);
        }
    }

    return err;
}

argon::object::ArObject *argon::vm::Panic(argon::object::ArObject *obj) {
    auto routine = GetRoutine();

    if (routine != nullptr)
        RoutineNewPanic(routine, obj);
    else {
        argon::object::Release(last_error);
        argon::object::IncRef(obj);
        last_error = obj;
    }

    return nullptr;
}

void argon::vm::SetRoutineMain(ArRoutine *routine) {
    routine_main = routine;
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

bool WireVC(VCore *vcore) {
    OSThread *current = vcore->ost.load(std::memory_order_consume);
    if (current == nullptr) {
        if (vcore->ost.compare_exchange_weak(current, ost_local, std::memory_order_relaxed)) {
            ost_local->current = vcore;
            vcore->status = VCoreStatus::RUNNING;
            vcs_idle_count--;
            return true;
        }
    }
    return false;
}

bool AcquireVCore() {
    bool ok = false;

    for (unsigned int i = 0; i < vcs_count; i++) {
        if ((ok = WireVC(vcs + i)))
            break;
    }

    return ok;
}

void ReleaseVC() {
    if (ost_local->current == nullptr)
        return;

    ost_local->old = ost_local->current;

    ost_local->current->status = VCoreStatus::IDLE;
    vcs_idle_count++;
    ost_local->current->ost = nullptr;

    ost_local->current = nullptr;
}

void OSTSleep() {
    // Release VCore
    ReleaseVC();

    // Go to sleep
    ost_cond_lock.lock();
    ost_cond.wait(ost_cond_lock);
    ost_cond_lock.unlock();

    // Reacquire VCore
    bool ok;
    do {
        if (!(ok = WireVC(ost_local->old)) && !should_stop)
            ok = AcquireVCore();
        if (!ok && !should_stop) {
            // Go back to sleep
            ost_cond_lock.lock();
            ost_cond.wait(ost_cond_lock);
            ost_cond_lock.unlock();
        }
    } while (!ok && !should_stop);
}

inline void OSTWakeup() {
    if (vcs_idle_count > 0)
        ost_cond.notify_one();
}

void ResetSpinning() {
    assert(ost_local->spinning);

    ost_local->spinning = false;
    ost_spinning_count--;

    OSTWakeup();
}

ArRoutine *FindExecutable() {
    ArRoutine *routine = nullptr;

    while (routine == nullptr && !should_stop) {
        // Local queue
        if ((routine = ost_local->current->queue.Dequeue()) != nullptr)
            return routine;

        // {   // Global queue
        // std::scoped_lock<std::mutex> scopedLock(gqr_lock);
        if ((routine = routine_gq.Dequeue()) != nullptr)
            return routine;
        // }

        //                                               ▼▼▼▼▼ Busy VCore ▼▼▼▼▼
        if (ost_local->spinning || ost_spinning_count < (vcs_count - vcs_idle_count)) {
            if (!ost_local->spinning) {
                ost_local->spinning = true;
                ost_spinning_count++;
            }

            // Steal work from other VCore
            auto vc = ost_local->current;
            unsigned short attempts = 3;
            do {
                // TODO: Use random start
                for (unsigned int i = 0; i < vcs_count; i++) {
                    auto target_vc = vcs + i;

                    if (vc == target_vc)
                        continue;

                    // Steal from queues that contain more than one item
                    routine = vc->queue.StealQueue(2, target_vc->queue);
                    if (routine != nullptr)
                        return routine;
                }
                attempts--;
            } while (attempts > 0);
        }

        OSTSleep();
    }

    return nullptr;
}

void Schedule(OSThread *self) {
    ost_local = self;

    start:

    if (should_stop)
        return;

    // Find a free VCore and associate with it
    bool ok = false;

    while (!ok && !should_stop)
        ok = AcquireVCore();

    if (ok)
        FromIdleToActive(self);

    // Main procedure
    while (!should_stop) {
        self->routine = FindExecutable();   // Blocks until returns

        if (self->spinning)
            ResetSpinning();

        // *******************************
        if (self->routine == nullptr) {
            if (should_stop)
                break;
            goto start;
        }
        // *******************************

        Eval(self->routine);

        // Free after use ? Yes (For now...)
        RoutineDel(self->routine);
        self->routine = nullptr;

        if (self->current == nullptr) {
            if (!WireVC(self->old)) {
                FromActiveToIdle(self);
                goto start;
            }
        }
    }

    // Shutdown procedure
    ReleaseVC();

    assert(self->routine == nullptr);

    ost_lock.lock();
    RemoveOSThread(self);
    ost_count--;
    ost_lock.unlock();

    self->self.detach();
    argon::memory::Free(self);
}

OSThread *AllocOST() {
    auto ost = (OSThread *) argon::memory::Alloc(sizeof(OSThread));

    if (ost != nullptr) {
        ost->next = nullptr;
        ost->prev = nullptr;

        ost->routine = nullptr;

        ost->current = nullptr;
        ost->old = nullptr;

        ost->spinning = false;

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