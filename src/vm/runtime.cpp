// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <util/macros.h>

#if defined (_ARGON_PLATFORM_DARWIN)

#include <mach-o/dyld.h>

#elif defined(_ARGON_PLATFORM_LINUX)

#include <unistd.h>

#elif defined(_ARGON_PLATFORM_WINDOWS)

#include <vm/support/nt/nt.h>

#endif

#include <atomic>
#include <cassert>
#include <random>

#include <lang/compiler_wrapper.h>

#include <vm/datatype/atom.h>
#include <vm/datatype/error.h>
#include <vm/datatype/future.h>
#include <vm/setup.h>

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
    bool stealing;
};

struct OSThread {
    OSThread *next;
    OSThread **prev;

    Fiber *fiber;
    FiberStatus fiber_status;

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

        if (vcore->next != nullptr)
            vcore->next->prev = vcore->prev;

        vcore->next = nullptr;
        vcore->prev = nullptr;
    }

    ost->current = vcore;
    ost->old = nullptr;

    vc_idle_count--;

    return true;
}

Fiber *AllocFiber(Context *context) {
    auto *fiber = fiber_pool.Dequeue();

    if (fiber != nullptr) {
        fiber->context = context;

        return fiber;
    }

    return FiberNew(context, fiber_stack_size);
}

Fiber *FindExecutable(bool lq_last) {
    auto *current = ost_local->current;
    Fiber *fiber;

    if (should_stop)
        return nullptr;

    if (!lq_last) {
        fiber = current->queue.Dequeue();
        if (fiber != nullptr)
            return fiber;
    }

    // Check from global queue
    if ((fiber = fiber_global.Dequeue()) != nullptr)
        return fiber;

    if ((fiber = StealWork(ost_local)) != nullptr)
        return fiber;

    if (lq_last) {
        fiber = current->queue.Dequeue();
        if (fiber != nullptr)
            return fiber;
    }

    return nullptr;
}

Fiber *StealWork(OSThread *ost) {
    static std::minstd_rand vc_random;

    auto *cur_vc = ost->current;

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

    auto start = r_distrib(vc_random);

    cur_vc->stealing = true;

    for (auto i = start; i < vc_total + (vc_total - start); i++) {
        auto *target_vc = vcores + (i % vc_total);
        if (target_vc == cur_vc || target_vc->stealing)
            continue;

        // Steal from queues that contain one or more items
        auto *fiber = cur_vc->queue.StealDequeue(1, target_vc->queue);
        if (fiber != nullptr) {
            cur_vc->stealing = false;
            return fiber;
        }
    }

    cur_vc->stealing = false;

    return nullptr;
}

OSThread *AllocOST() {
    auto *ost = (OSThread *) memory::Calloc(sizeof(OSThread));

    if (ost != nullptr)
        new(&ost->self) std::thread();

    return ost;
}

void AcquireOrSuspend(OSThread *ost, Fiber **last) {
    std::unique_lock lock(vc_lock);

    while (ost->current == nullptr) {
        if (WireVCore(ost, ost->old) || AcquireVCore(ost)) {
            lock.unlock();

            OSTIdle2Active(ost);
            break;
        }

        if (*last != nullptr) {
            fiber_global.Enqueue(*last);
            *last = nullptr;
        }

        lock.unlock();
        OSTSleep();
        lock.lock();
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

    if (fiber_global.IsEmpty() && vc_idle_count == 0)
        return;

    std::unique_lock o_lock(ost_lock);

    if (ost_idle != nullptr) {
        ost_cond.notify_one();
        return;
    }

    if ((ost_total + 1) > ost_max)
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

            FutureSetResult(fiber->future, nullptr, result);
            return;
        }

        FutureSetResult(fiber->future, result, nullptr);
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

        SetFiberStatus(FiberStatus::RUNNING);

        result = Eval(self->fiber);

        if (self->fiber_status != FiberStatus::RUNNING) {
            if (self->fiber_status == FiberStatus::SUSPENDED)
                last = self->fiber;

            continue;
        }

        assert(self->fiber->frame == nullptr);
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

    if (!current->queue.IsEmpty()) {
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

Future *argon::vm::EvalAsync(Function *func, ArObject **argv, ArSize argc, OpCodeCallMode mode) {
    Fiber *fiber;

    assert(ost_local != nullptr);

    fiber = AllocFiber(GetFiber()->context);
    if (fiber == nullptr)
        return nullptr;

    auto *frame = FrameNew(fiber, func, argv, argc, mode);
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

    fiber->future = IncRef(future);
    fiber->frame = frame;

    fiber_global.Enqueue(fiber);

    OSTWakeRun();

    return future;
}

Result *argon::vm::Eval(Context *context, Code *code, Namespace *ns) {
    auto *fiber = AllocFiber(context);
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

    fiber->future = IncRef(future);
    fiber->frame = frame;

    fiber_global.Enqueue(fiber);

    OSTWakeRun();

    ON_ARGON_CONTEXT Yield();

    FutureWait(future);

    auto *result = FutureResult(future);

    Release(future);

    return result;
}

Result *argon::vm::EvalFile(Context *context, const char *name, const char *path, Namespace *ns) {
    lang::CompilerWrapper c_wrapper;
    FILE *f;

    if ((f = fopen(path, "r")) == nullptr) {
        ErrorFromErrno(errno);
        return nullptr;
    }

    auto *code = c_wrapper.Compile(name, f);

    fclose(f);

    if (code == nullptr)
        return nullptr;

    auto *result = Eval(context, code, ns);

    Release(code);

    return result;
}

Result *argon::vm::EvalString(Context *context, const char *name, const char *source, Namespace *ns) {
    lang::CompilerWrapper c_wrapper;

    auto *code = c_wrapper.Compile(name, source);
    if (code == nullptr)
        return nullptr;

    auto *result = Eval(context, code, ns);

    Release(code);

    return result;
}

argon::vm::datatype::String *argon::vm::GetExecutableName() {
    unsigned long size = 1024;
    char *path_buf;

    String *path;

    if ((path_buf = (char *) memory::Alloc(size)) == nullptr)
        return nullptr;

#if defined(_ARGON_PLATFORM_WINDOWS)

    size = support::nt::GetExecutablePath(path_buf, (int) size);

#elif defined(_ARGON_PLATFORM_LINUX)

    size = readlink("/proc/self/exe", path_buf, size);

#elif defined(_ARGON_PLATFORM_DARWIN)

    size = _NSGetExecutablePath(path_buf, (unsigned int *) &size);

#endif

    if (size != -1)
        path = StringNew(path_buf);
    else
        path = StringIntern("");

    memory::Free(path_buf);

    return path;
}

argon::vm::datatype::String *argon::vm::GetExecutablePath() {
    auto *name = GetExecutableName();
    String *ret;

    ArSSize idx;

    if (name == nullptr)
        return nullptr;

    idx = StringRFind(name, _ARGON_PLATFORM_PATHSEP);

    if (idx >= 0) {
        ret = StringSubs(name, 0, idx);

        Release(name);

        return ret;
    }

    return name;
}

bool argon::vm::CheckLastPanic(const char *id) {
    auto **last = &panic_global;

    ON_ARGON_CONTEXT last = &ost_local->fiber->panic;

    if (last == nullptr || !AR_TYPEOF((*last)->object, type_error_))
        return false;

    return datatype::AtomCompareID(((Error *) (*last)->object)->id, id);
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

bool argon::vm::IsPanickingFrame() {
    ON_ARGON_CONTEXT {
        if (ost_local->fiber->panic == nullptr)
            return false;

        return (uintptr_t) ost_local->fiber->frame == ost_local->fiber->panic->gen_id;
    }

    assert(false);
}

bool argon::vm::Shutdown() {
    short attempt = 10;

    should_stop = true;
    ost_cond.notify_all();

    while (ost_total > 0 && attempt > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        attempt--;
    }

    return ost_total == 0;
}

bool argon::vm::Spawn(Function *func, ArObject **argv, ArSize argc, OpCodeCallMode mode) {
    Fiber *fiber;

    assert(ost_local != nullptr);

    fiber = AllocFiber(GetFiber()->context);
    if (fiber == nullptr)
        return false;

    auto *frame = FrameNew(fiber, func, argv, argc, mode);
    if (frame == nullptr) {
        FreeFiber(fiber);
        return false;
    }

    FiberPushFrame(fiber, frame);

    fiber_global.Enqueue(fiber);

    OSTWakeRun();

    return true;
}

Fiber *argon::vm::GetFiber() {
    ON_ARGON_CONTEXT return ost_local->fiber;

    return nullptr;
}

FiberStatus argon::vm::GetFiberStatus() {
    ON_ARGON_CONTEXT return ost_local->fiber_status;

    assert(false);
}

Frame *argon::vm::GetFrame() {
    ON_ARGON_CONTEXT return ost_local->fiber->frame;

    return nullptr;
}

void argon::vm::Cleanup() {
    if (ost_total == 0) {
        for (unsigned int i = 0; i < vc_total; i++)
            (vcores + i)->queue.~FiberQueue();

        memory::MemoryFinalize();
    }
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

        ost_local->fiber->panic->gen_id = (uintptr_t) ost_local->fiber->frame;

        return;
    }

    if ((panic_global = PanicNew(panic_global, panic)) == nullptr)
        PanicOOM(&panic_global, panic);
}

void argon::vm::SetFiberStatus(FiberStatus status) {
    ON_ARGON_CONTEXT {
        ost_local->fiber->status = status;
        ost_local->fiber_status = status;
    }
}

void argon::vm::Spawn(argon::vm::Fiber *fiber) {
    fiber->status = FiberStatus::RUNNABLE;

    fiber_global.Enqueue(fiber);

    OSTWakeRun();
}

void argon::vm::Yield() {
    if (ost_local == nullptr || ost_local->current == nullptr)
        return;

    SetFiberStatus(FiberStatus::SUSPENDED);

    bool has_work = !ost_local->current->queue.IsEmpty();

    VCoreRelease(ost_local);

    if (has_work)
        OSTWakeRun();
}