// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <atomic>
#include <cassert>
#include <random>

#include <vm/datatype/future.h>
#include <vm/datatype/setup.h>

#include "areval.h"
#include "fiber.h"
#include "fqueue.h"
#include "runtime.h"

using namespace argon::vm;
using namespace argon::vm::datatype;

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

    bool spinning;

    std::thread self;
};

// OSThread variables
OSThread *ost_active = nullptr;             // Working OSThread
OSThread *ost_idle = nullptr;               // IDLE OSThread
thread_local OSThread *ost_local = nullptr; // OSThread for actual thread

unsigned int ost_total = 0;                 // OSThread counter
unsigned int ost_idle_count = 0;            // OSThread counter (idle)
unsigned int ost_max = 0;                   // Maximum OS thread allowed

std::atomic_uint ost_spinning_count = 0;    // OSThread in spinning
std::atomic_uint ost_worker_count = 0;      // OSThread counter (worker)

bool should_stop = false;

std::mutex ost_lock;
std::condition_variable ost_cond;

// VCore variables
VCore *vcores = nullptr;                    // List of instantiated VCore
VCore *vcores_active = nullptr;             // List of suspended VCores that have at least 1 ArRoutine in the queue

unsigned int vc_total = 0;                  // Maximum concurrent VCore

std::atomic_uint vc_idle_count = 0;         // IDLE VCore

std::mutex vc_lock;

unsigned int fiber_stack_size = 0;          // Fiber stack size

// Panic management
struct Panic *panic_global = nullptr;
std::atomic<struct Panic *> panic_oom = nullptr;

// Global queues
FiberQueue fiber_global;
FiberQueue fiber_pool;

// Prototypes

bool WireVCore(OSThread *, VCore *);

Fiber *StealWork(OSThread *);

void OSTIdle2Active(OSThread *);

void OSTRemove(OSThread *);

void OSTSleep();

void PushOSThread(OSThread **, OSThread *);

void Scheduler(OSThread *);

void VCoreRelease(OSThread *);

// Internal

#define ON_ARGON_CONTEXT                    \
    if (ost_local != nullptr)

#define PUSH_LCQUEUE(vcore, fiber)          \
    do {                                    \
        if(!(vcore)->queue.Enqueue(fiber))  \
            fiber_global.Enqueue(fiber);    \
    } while(0)

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
        new(&(vcores + i)->queue)FiberQueue(kVCoreQueueLengthMax);

    vc_total = n;
    vc_idle_count = n;

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

    vc_idle_count--;

    return true;
}

Fiber *AllocFiber() {
    auto *fiber = fiber_pool.Dequeue();
    if (fiber != nullptr)
        return fiber;

    return FiberNew(fiber_stack_size);
}

Fiber *FindExecutable(bool ignore_local) {
    auto *current = ost_local->current;
    Fiber *fiber;

    if (should_stop)
        return nullptr;

    if (!ignore_local) {
        // Check from local queue
        fiber = current->queue.Dequeue();
        if (fiber != nullptr)
            return fiber;
    }

    // Check from global queue
    if ((fiber = fiber_global.Dequeue()) != nullptr)
        return fiber;

    if ((fiber = StealWork(ost_local)) != nullptr)
        return fiber;

    return nullptr;
}

Fiber *StealWork(OSThread *ost) {
    static std::minstd_rand vc_random;

    auto *cur_vc = ost->current;

    auto attempts = kOSTStealWorkAttempts;

    std::unique_lock lock(ost_lock);

    //                                                  ▼▼▼▼▼ Busy VCore ▼▼▼▼▼
    if (!ost->spinning && (ost_spinning_count + 1) > (vc_total - vc_idle_count))
        return nullptr;

    if (!ost->spinning) {
        ost->spinning = true;
        ost_spinning_count++;
    }

    lock.unlock();

    // Steal work from other VCore
    std::uniform_int_distribution<unsigned int> r_distrib(0, vc_total);
    do {
        auto start = r_distrib(vc_random);

        for (auto i = start; i < vc_total; i++) {
            auto *target_vc = vcores + i;
            if (target_vc == cur_vc)
                continue;

            // Steal from queues that contain more than one item
            auto *fiber = cur_vc->queue.StealDequeue(1, target_vc->queue);
            if (fiber != nullptr)
                return fiber;
        }
    } while (--attempts > 0);

    return nullptr;
}

OSThread *AllocOST() {
    auto *ost = (OSThread *) memory::Calloc(sizeof(OSThread));

    if (ost != nullptr)
        new(&ost->self) std::thread();

    return ost;
}

void AcquireOrSuspend(OSThread *ost, Fiber **last) {
    if (*last != nullptr) {
        assert(ost->old != nullptr);

        PUSH_LCQUEUE(ost->old, *last);

        *last = nullptr;
    }

    while (ost->current == nullptr) {
        std::unique_lock lock(vc_lock);

        if (WireVCore(ost, ost->old) || AcquireVCore(ost)) {
            lock.unlock();

            OSTIdle2Active(ost);

            break;
        }

        OSTSleep();
    }
}

void FreeFiber(Fiber *fiber) {
    Release((ArObject **) &fiber->future);

    if (!fiber_pool.Enqueue(fiber))
        FiberDel(fiber);
}

void FreeOSThread(OSThread *ost) {
    if (ost != nullptr) {
        ost->self.~thread();
        memory::Free(ost);
    }
}

void OSTActive2Idle(OSThread *ost) {
    std::unique_lock lock(ost_lock);

    if (ost->current != nullptr)
        VCoreRelease(ost);

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

    if (fiber_global.IsEmpty() || vc_idle_count == 0)
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

        if (panic_oom.compare_exchange_strong(expected, cursor))
            continue;

        memory::Free(cursor);
    }
}

void PanicOOM(struct Panic **panic, ArObject *object) {
    auto *tmp = panic_oom.exchange(nullptr);

    assert(tmp != nullptr);

    tmp->panic = *panic;
    tmp->object = IncRef(object);
    tmp->recovered = false;
    tmp->aborted = *panic != nullptr;

    *panic = tmp;
}

void PublishResult(Fiber *fiber, ArObject *result) {
    if (fiber->future != nullptr) {
        if (result == nullptr) {
            result = GetLastError();

            FutureResult(fiber->future, nullptr, result);
            return;
        }

        FutureResult(fiber->future, result, nullptr);
    } else {
        // TODO: else PrintStackTrace!
    }

    FreeFiber(fiber);
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

void ResetSpinning(OSThread *ost) {
    ost->spinning = false;
    ost_spinning_count--;

    if (vc_idle_count > 0)
        ost_cond.notify_one();
}

void Scheduler(OSThread *self) {
    ArObject *result;
    Fiber *last = nullptr;

    unsigned int tick = 0;

    ost_local = self;

    while (!should_stop) {
        AcquireOrSuspend(self, &last);

        if (++tick >= kScheduleTickBeforeCheck) {
            self->fiber = FindExecutable(true);
            tick = 0;
        } else
            self->fiber = FindExecutable(false);

        if (self->fiber == nullptr) {
            if (last == nullptr) {
                OSTActive2Idle(self);
                OSTSleep();

                continue;
            }

            self->fiber = last;
            last = nullptr;
        }

        if (last != nullptr) {
            PUSH_LCQUEUE(self->current, last);
            last = nullptr;
        }

        if (self->spinning)
            ResetSpinning(self);

        self->fiber->status = FiberStatus::RUNNING;

        result = Eval(self->fiber);

        if (self->fiber->status != FiberStatus::RUNNING) {
            if (self->fiber->status == FiberStatus::SUSPENDED)
                last = self->fiber;

            continue;
        }

        PublishResult(self->fiber, result);
        Release(result);
    }

    // Shutdown thread
    assert(self->fiber == nullptr);

    OSTActive2Idle(self);

    std::unique_lock lock(ost_lock);
    OSTRemove(self);
    self->self.detach();
    FreeOSThread(self);
    ost_total--;
}

void VCoreRelease(OSThread *ost) {
    auto *current = ost->current;

    if (ost->current == nullptr)
        return;

    ost->old = ost->current;
    ost->current = nullptr;

    if (current->queue.IsEmpty()) {
        std::unique_lock lock(vc_lock);

        auto *next = &vcores_active;

        while (*next != nullptr)
            next = &((*next)->next);

        *next = current;
        current->prev = next;
    }

    current->wired = false;
    vc_idle_count++;
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

    fiber->future = future;
    fiber->frame = frame;

    fiber_global.Enqueue(fiber);

    OSTWakeRun();

    FutureWait(future);

    auto *result = IncRef(future->success);
    if (future->success == nullptr)
        result = IncRef(future->error);

    Release(future);

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
