// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <atomic>
#include <cassert>

#include <vm/datatype/future.h>
#include <vm/datatype/setup.h>

#include "fiber.h"
#include "fqueue.h"
#include "runtime.h"

using namespace argon::vm;
using namespace argon::vm::datatype;

#define ON_ARGON_CONTEXT if (ost_local != nullptr)

struct VCore {
    VCore *next;
    VCore **prev;

    FiberQueue queue;

    bool wired;
};

struct OSThread {
    OSThread *next;
    OSThread **prev;

    Fiber *fiber;

    VCore *current;
    VCore *old;

    std::thread self;
};

// OSThread variables
OSThread *ost_active = nullptr;             // Working OSThread
OSThread *ost_idle = nullptr;               // IDLE OSThread
thread_local OSThread *ost_local = nullptr; // OSThread for actual thread

unsigned int ost_total = 0;                 // OSThread counter
unsigned int ost_idle_count = 0;            // OSThread counter (idle)
unsigned int ost_max = 0;
std::atomic_uint ost_worker_count = 0;      // OSThread counter (worker)

std::mutex ost_lock;

// VCore variables
VCore *vcores = nullptr;                    // List of instantiated VCore
VCore *vcores_active = nullptr;             // List of suspended VCores that have at least 1 ArRoutine in the queue
unsigned int vc_total = 0;                  // Maximum concurrent VCore
std::atomic_uint vc_idle_count = 0;         // IDLE VCore

std::mutex vc_lock;

std::condition_variable ost_cond;

unsigned int fiber_stack_size = 0;

// Panic management
struct Panic *panic_global = nullptr;
std::atomic<struct Panic *> *panic_oom = nullptr;

// Global queues
FiberQueue fiber_global;
FiberQueue fiber_pool;

// Prototypes

bool WireVCore(OSThread *, VCore *);

void OSTRemove(OSThread *ost);

void PushOSThread(OSThread **, OSThread *);

void Scheduler(OSThread *);

// Internal

bool AcquireVCore(OSThread *ost) {
    for (VCore *cursor = vcores_active; cursor != nullptr; cursor = cursor->next) {
        if (WireVCore(ost, cursor))
            return true;
    }

    for (unsigned int i = 0; i < vc_total; i++) {
        if (WireVCore(ost, vcores + i))
            return true;
    }

    return false;
}

bool InitializeVCores(unsigned int n) {
    if (n == 0) {
        n = std::thread::hardware_concurrency();
        if (n == 0)
            n = kVCoreDefault;
    }

    if ((vcores = (VCore *) memory::Calloc(sizeof(VCore) * n)) == nullptr)
        return false;

    for (unsigned int i = 0; i < n; i++)
        new(&(vcores + i)->queue)FiberQueue();

    vc_total = n;

    return true;
}

bool WireVCore(OSThread *ost, VCore *vcore) {
    if (vcore == nullptr || vcore->wired)
        return false;

    vcore->wired = true;

    if (vcore->prev != nullptr) {
        *(vcore->prev) = vcore->next;
        vcore->next->prev = vcore->prev;

        vcore->next = nullptr;
        vcore->prev = nullptr;
    }

    ost->current = vcore;

    vc_idle_count++;

    return true;
}

Fiber *AllocFiber() {
    auto *fiber = fiber_pool.Dequeue();
    if (fiber != nullptr)
        return fiber;

    return FiberNew(fiber_stack_size);
}

OSThread *AllocOST() {
    auto *ost = (OSThread *) memory::Calloc(sizeof(OSThread));

    if (ost != nullptr)
        new(&ost->self) std::thread();

    return ost;
}

void FreeFiber(Fiber *fiber) {
    Release((ArObject **) &fiber->future);

    if (!fiber_pool.Enqueue(fiber))
        FiberDel(fiber);
}

void OSTActive2Idle(OSThread *ost) {
    std::unique_lock lock(ost_lock);
    OSTRemove(ost);
    PushOSThread(&ost_idle, ost);
    ost_idle_count++;
    ost_worker_count--;
}

void OSTIdle2Active(OSThread *ost) {
    std::unique_lock lock(ost_lock);
    OSTRemove(ost);
    PushOSThread(&ost_active, ost);
    ost_idle_count--;
    ost_worker_count++;
}

void OSTRemove(OSThread *ost) {
    if (ost->prev != nullptr)
        *ost->prev = ost->next;
    if (ost->next != nullptr)
        ost->next->prev = ost->prev;
}

void OSTSleep() {
    std::unique_lock lock(ost_lock);
    ost_cond.wait(lock); // Wait forever
}

void OSTWakeRun() {
    OSThread *ost;
    bool acquired;

    std::unique_lock v_lock(vc_lock);

    if (fiber_global.IsEmpty() && vcores_active == nullptr)
        return;

    std::unique_lock o_lock(ost_lock);

    if (ost_idle != nullptr) {
        ost_cond.notify_one();
        return;
    }

    if (ost_total > ost_max)
        return;

    if ((ost = AllocOST()) == nullptr) {
        v_lock.unlock();

        // TODO error!
        assert(false);
    }

    ost_total++;

    acquired = AcquireVCore(ost);
    v_lock.unlock();

    if (!acquired) {
        PushOSThread(&ost_idle, ost);
        ost_idle_count++;
    } else
        PushOSThread(&ost_active, ost);

    ost->self = std::thread(Scheduler, ost);
}

void PanicCleanup(struct Panic **panic) {
    struct Panic *expected = nullptr;

    while (*panic != nullptr) {
        auto *cursor = *panic;

        *panic = cursor->panic;

        Release(cursor->object);

        if (panic_oom->compare_exchange_strong(expected, cursor))
            continue;

        memory::Free(cursor);
    }
}

void PanicOOM(struct Panic **panic, ArObject *object) {
    auto *tmp = panic_oom->exchange(nullptr);

    assert(tmp != nullptr);

    tmp->panic = *panic;
    tmp->object = IncRef(object);
    tmp->recovered = false;
    tmp->aborted = *panic != nullptr;

    *panic = tmp;
}

void PushOSThread(OSThread **list, OSThread *ost) {
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

void Scheduler(OSThread *self) {
    ost_local = self;

    while (self->current == nullptr) {
        std::unique_lock lock(vc_lock);

        if (WireVCore(self, self->old) || AcquireVCore(self)) {
            lock.unlock();

            OSTIdle2Active(self);

            break;
        }

        OSTSleep();
    }
}

// Public

ArObject *argon::vm::Eval(Code *code, Namespace *ns) {
    auto *fiber = AllocFiber();
    if (fiber == nullptr)
        return nullptr;

    auto *frame = FrameNew(fiber, code, ns, false);
    if (frame == nullptr) {
        FreeFiber(fiber);
        return nullptr;
    }

    // Set future
    auto *future = FutureNew();
    if (future == nullptr) {
        FrameDel(frame);
        FreeFiber(fiber);
        return nullptr;
    }

    fiber_global.Enqueue(fiber);

    OSTWakeRun();

    FutureWait(future);

    auto *result = FutureResult(future);

    FrameDel(frame);
    FreeFiber(fiber);

    return result;
}

ArObject *argon::vm::GetLastError() {
    ArObject *error;

    ON_ARGON_CONTEXT {
        if (ost_local->fiber->panic == nullptr)
            return nullptr;

        error = IncRef(ost_local->fiber->panic->object);

        PanicCleanup(&ost_local->fiber->panic);

        return error;
    }

    if (panic_global == nullptr)
        return nullptr;

    error = IncRef(panic_global->object);

    PanicCleanup(&panic_global);

    return error;
}

bool argon::vm::Initialize(const Config *config) {
    if (!memory::MemoryInit())
        return false;

    if (!InitializeVCores(config->max_vc)) {
        memory::MemoryFinalize();
        return false;
    }

    fiber_stack_size = config->fiber_ss;
    if (config->fiber_ss < 0)
        fiber_stack_size = kFiberStackSize;

    fiber_pool.SetLimit(config->fiber_pool);
    if (config->fiber_pool < 0)
        fiber_pool.SetLimit(kFiberPoolSize);

    ost_max = config->max_ost;
    if (config->max_ost <= 0)
        ost_max = kOSThreadMax;

    if (!Setup())
        return false;

    // TODO: panic_oom

    return true;
}

bool argon::vm::IsPanicking() {
    ON_ARGON_CONTEXT return ost_local->fiber->panic != nullptr;

    return panic_global != nullptr;
}

Fiber *argon::vm::GetFiber() {
    ON_ARGON_CONTEXT return ost_local->fiber;

    return nullptr;
}

Frame *argon::vm::GetFrame() {
    ON_ARGON_CONTEXT return ost_local->fiber->frame;

    return nullptr;
}

void argon::vm::DiscardLastPanic() {
    ON_ARGON_CONTEXT {
        PanicCleanup(&ost_local->fiber->panic);
        return;
    }

    if (panic_global != nullptr)
        PanicCleanup(&panic_global);
}

void argon::vm::Panic(datatype::ArObject *panic) {
    ON_ARGON_CONTEXT {
        if ((ost_local->fiber->panic = PanicNew(ost_local->fiber->panic, panic)) == nullptr)
            PanicOOM(&ost_local->fiber->panic, panic);

        return;
    }

    if ((panic_global = PanicNew(panic_global, panic)) == nullptr)
        PanicOOM(&panic_global, panic);
}
