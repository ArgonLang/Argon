// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <thread>
#include <condition_variable>
#include <cstdarg>

#include <object/setup.h>
#include <object/datatype/error.h>

#include "routinequeue.h"
#include "runtime.h"
#include "areval.h"

using namespace argon::object;
using namespace argon::vm;

#define VCORE_DEFAULT   2
#define OST_MAX         10000

enum class VCoreStatus {
    IDLE,
    RUNNING
};

struct VCore {
    VCore *next;
    VCore **prev;

    struct OSThread *ost;

    ArRoutineQueue queue;   // ArRoutine queue (No lock needed... In the future, I hope ...))

    VCoreStatus status;
};

struct OSThread {
    OSThread *next;
    OSThread **prev;

    ArRoutine *routine;

    VCore *current;
    VCore *old;

    bool spinning;
    bool ignore_routine;

    std::thread self;
};

// OsThread variables
OSThread *ost_active = nullptr;             // Working OSThread
OSThread *ost_idle = nullptr;               // IDLE OSThread
thread_local OSThread *ost_local = nullptr; // OSThread for actual thread
thread_local OSThread *ost_main = nullptr;  // OSThread main thread

unsigned int ost_total = 0;                 // OSThread counter
unsigned int ost_idle_count = 0;            // OSThread counter (idle)
std::atomic_uint ost_spinning_count = 0;    // OSThread in spinning
std::atomic_int ost_worker_count = 0;       // OSThread counter (worker)

bool should_stop = false;                   // If true all thread should be stopped
bool stw = false;

std::mutex ost_lock;
std::mutex stw_lock;

std::condition_variable ost_cond;
std::condition_variable cond_stw;
std::condition_variable cond_stopwait;

// VCore variables
VCore *vcores = nullptr;                    // List of instantiated VCore
VCore *vcores_active = nullptr;             // List of suspended VCores that have at least 1 ArRoutine in the queue
unsigned int vc_total = 0;                  // Maximum concurrent VCore
std::atomic_uint vc_idle_count = 0;         // IDLE VCore

std::mutex vc_lock;

/* ArRoutine Global queue */
ArRoutineQueue routine_gq;

ArRoutine *FindExecutable() {
    ArRoutine *routine;
    VCore *target_vc;
    VCore *self_vc;

    unsigned short attempts = 3;

    if (should_stop)
        return nullptr;

    // Check from local queue
    if ((routine = ost_local->current->queue.Dequeue()) != nullptr)
        return routine;

    // Check from global queue
    if ((routine = routine_gq.Dequeue()) != nullptr)
        return routine;

    // Try to steal work from another thread
    //                                                  ▼▼▼▼▼ Busy VCore ▼▼▼▼▼
    if (ost_local->spinning || ost_spinning_count < (vc_total - vc_idle_count)) {
        if (!ost_local->spinning) {
            ost_local->spinning = true;
            ost_spinning_count++;
        }

        // Steal work from other VCore
        self_vc = ost_local->current;
        do {
            // Scan other VCores
            for (unsigned int i = 0; i < vc_total; i++) {
                target_vc = vcores + i;

                if (target_vc == self_vc)
                    continue;

                // Steal from queues that contain more than one item
                if ((routine = self_vc->queue.StealQueue(2, target_vc->queue)) != nullptr)
                    return routine;
            }
        } while (--attempts > 0);
    }

    return routine;
}

bool WireVCore(VCore *vcore) {
    bool ok = false;

    if (vcore != nullptr && vcore->ost == nullptr) {
        vcore->ost = ost_local;
        vcore->status = VCoreStatus::RUNNING;

        if (vcore->prev != nullptr) {
            *(vcore->prev) = vcore->next;
            vcore->next->prev = vcore->prev;

            vcore->next = nullptr;
            vcore->prev = nullptr;
        }

        ost_local->current = vcore;
        vc_idle_count--;
        ok = true;
    }

    return ok;
}

bool AcquireVCore() {
    std::unique_lock lck(vc_lock);

    for (VCore *cursor = vcores_active; cursor != nullptr; cursor = cursor->next) {
        if (WireVCore(cursor))
            return true;
    }

    for (unsigned int i = 0; i < vc_total; i++) {
        if ((WireVCore(vcores + i)))
            return true;
    }

    return false;
}

bool InitializeVCores() {
    if ((vcores = (VCore *) argon::memory::Alloc(sizeof(VCore) * vc_total)) == nullptr)
        return false;

    for (unsigned int i = 0; i < vc_total; i++) {
        vcores[i].next = nullptr;
        vcores[i].prev = nullptr;
        vcores[i].ost = nullptr;
        new(&((vcores + i)->queue))ArRoutineQueue(ARGON_VM_QUEUE_MAX_ROUTINES);
        vcores[i].status = VCoreStatus::IDLE;
    }

    return true;
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
        ost->ignore_routine = false;

        new(&ost->self) std::thread();
    }

    return ost;
}

inline void FreeOST(OSThread *ost) {
    if (ost != nullptr) {
        ost->self.~thread();
        argon::memory::Free(ost);
    }
}

void OSTSleep() {
    std::unique_lock lck(ost_lock);
    ost_cond.wait(lck);
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

void ReleaseVCore() {
    if (ost_local->current == nullptr)
        return;

    ost_local->old = ost_local->current;
    ost_local->current->status = VCoreStatus::IDLE;

    if (ost_local->current->queue.Length() > 0) {
        std::unique_lock lck(vc_lock);
        VCore **next = &vcores_active;

        while (*next != nullptr)
            next = &((*next)->next);

        *next = ost_local->current;
        ost_local->current->prev = next;
    }

    ost_local->current->ost = nullptr;
    ost_local->current = nullptr;

    vc_idle_count++;
}

void RemoveOSThread(OSThread *ost) {
    if (ost->prev != nullptr)
        *ost->prev = ost->next;
    if (ost->next != nullptr)
        ost->next->prev = ost->prev;
}

void ResetSpinning() {
    ost_local->spinning = false;
    ost_spinning_count--;

    if (vc_idle_count > 0)
        ost_cond.notify_one();
}

void FromActiveToIdle(OSThread *ost) {
    if (ost_worker_count.fetch_sub(1) == 1)
        cond_stw.notify_one();

    if (ost->current != nullptr)
        ReleaseVCore();

    ost_lock.lock();
    RemoveOSThread(ost);
    PushOSThread(ost, &ost_idle);
    ost_idle_count++;
    ost_lock.unlock();
}

void FromIdleToActive(OSThread *ost) {
    ost_lock.lock();
    RemoveOSThread(ost);
    PushOSThread(ost, &ost_active);
    ost_idle_count--;
    ost_lock.unlock();

    ost_worker_count++;
}

void Schedule(OSThread *self) {
    ArRoutine *routine;
    ost_local = self;

    START:

    // Find a free VCore and associate with it
    while (!should_stop) {
        {
            std::unique_lock lck(vc_lock);
            if (WireVCore(ost_local->old))
                break;
        }

        if (AcquireVCore())
            break;

        OSTSleep();
    }

    FromIdleToActive(self);

    // Main dispatch procedure
    while (!should_stop) {
        if ((self->routine = FindExecutable()) == nullptr) {
            FromActiveToIdle(self);
            OSTSleep();
            goto START;
        }

        if (self->spinning)
            ResetSpinning();

        if (should_stop) {
            RoutineDel(self->routine);
            self->routine = nullptr;
            break;
        }

        while (self->routine->status != ArRoutineStatus::RUNNING) {
            self->routine->status = ArRoutineStatus::RUNNING;
            Release(Eval(self->routine));

            if (self->ignore_routine) {
                self->routine = nullptr;
                self->ignore_routine = false;
                break;
            }

            STWCheckpoint();

            if (self->routine->status != ArRoutineStatus::RUNNING) {
                if (self->current == nullptr) {
                    std::this_thread::yield();
                    continue;
                }

                // Local queue
                routine = ost_local->current->queue.Dequeue();

                if (routine == nullptr) {
                    routine = FindExecutable();

                    if (self->spinning)
                        ResetSpinning();
                }

                if (routine != nullptr) {
                    self->current->queue.Enqueue(self->routine);
                    self->routine = routine;
                }
            }
        }

        RoutineDel(self->routine);
        self->routine = nullptr;

        if (self->current == nullptr) {
            FromActiveToIdle(self);
            goto START;
        } else
            STWCheckpoint();
    }

    // Shutdown thread
    assert(self->routine == nullptr);

    FromActiveToIdle(self);

    ost_lock.lock();
    RemoveOSThread(self);
    self->self.detach();
    FreeOST(self);
    ost_total--;
    ost_lock.unlock();
}

void OSTWakeRun() {
    std::unique_lock lck(ost_lock);
    OSThread *ost;

    if (routine_gq.Length() == 0 && vcores_active == nullptr)
        return;

    if (ost_idle_count > 0) {
        ost_cond.notify_one();
        return;
    }

    if (ost_total > OST_MAX)
        return;

    if ((ost = AllocOST()) != nullptr) {
        PushOSThread(ost, &ost_idle);
        ost_total++;
        ost_idle_count++;

        ost_lock.unlock();
        ost->self = std::thread(Schedule, ost);
    }

    // TODO: Signal Error?!
}

ArObject *argon::vm::Call(ArObject *callable, int argc, ArObject **args) {
    auto func = (Function *) callable;
    ArObject *result = nullptr;
    Frame *frame;

    if (!AR_TYPEOF(callable, type_function_))
        return ErrorFormat(type_type_error_, "'%s' object is not callable", AR_TYPE_NAME(func));

    if (func->IsNative())
        return FunctionCallNative(func, args, argc);

    if ((frame = FrameNew(func->code, func->gns, nullptr)) != nullptr) {
        FrameFillForCall(frame, func, args, argc);
        result = Eval(GetRoutine(), frame);
        FrameDel(frame);
    }

    if (IsPanicking())
        Release((ArObject **) &result);

    return result;
}

ArObject *argon::vm::Call(ArObject *callable, int argc, ...) {
    ArObject **args;
    ArObject *result;

    va_list varargs;

    if ((args = (ArObject **) memory::Alloc(argc * sizeof(void *))) == nullptr)
        return Panic(error_out_of_memory);

    va_start(varargs, argc);

    for (int i = 0; i < argc; i++)
        args[i] = va_arg(varargs, ArObject*);

    va_end(varargs);

    result = Call(callable, argc, args);
    memory::Free(args);

    return result;
}

ArObject *argon::vm::CallMethod(ArObject *instance, const char *name, int argc, ArObject **args) {
    ArObject *ret;
    ArObject *meth;
    bool meth_found;

    if ((meth = InstanceGetMethod(instance, name, &meth_found)) == nullptr)
        return nullptr;

    ret = Call(meth, argc, args);
    Release(meth);

    return ret;
}

ArObject *argon::vm::GetLastError() {
    ArRoutine *routine = GetRoutine();
    ArObject *err = nullptr;

    if (routine->panic != nullptr)
        err = RoutineRecover(routine);

    return err;
}

ArObject *argon::vm::Panic(ArObject *obj) {
    RoutineNewPanic(GetRoutine(), obj);
    return nullptr;
}

ArRoutine *argon::vm::GetRoutine() {
    if (ost_main != nullptr)
        return ost_main->routine;

    return ost_local->routine;
}

ArRoutine *argon::vm::UnschedRoutine(bool resume_last, unsigned long reason) {
    ArRoutine *routine = GetRoutine();
    ArRoutineStatus status = ArRoutineStatus::SUSPENDED;

    if (resume_last) {
        status = ArRoutineStatus::BLOCKED;
        routine->reason = reason;
    }

    if (!CanSpin())
        return nullptr;

    routine->status = status;

    ost_local->ignore_routine = true;
    return routine;
}

Context *argon::vm::GetContext() {
    return GetRoutine()->context;
}

bool argon::vm::AcquireMain() {
    if (ost_main == nullptr)
        return false;

    GetRoutine()->status = ArRoutineStatus::RUNNING;
    ost_worker_count++;

    return true;
}

bool argon::vm::CanSpin() {
    if (ost_main != nullptr)
        return false;

    // Check for a nested, non-interruptible call to Eval
    for (Frame *cursor = GetRoutine()->frame->back; cursor != nullptr; cursor = cursor->back) {
        if (FRAME_TAGGED(cursor))
            return false;
    }

    return true;
}

bool argon::vm::DiscardErrorType(const TypeInfo *type) {
    ArRoutine *routine = GetRoutine();

    if (routine->panic != nullptr) {
        if (!AR_TYPEOF(routine->panic->object, type))
            return false;

        RoutinePopPanics(routine);
    }

    return true;
}

bool argon::vm::IsPanicking() {
    return GetRoutine()->panic != nullptr;
}

bool argon::vm::Initialize() {
    if (vc_total == 0)
        SetVCoreTotal(0);

    vc_idle_count = vc_total;

    // Initialize memory subsystem
    argon::memory::InitializeMemory();

    if (!InitializeVCores())
        goto ERROR;

    // Setup default OSThread
    if ((ost_main = AllocOST()) == nullptr)
        goto ERROR;

    // Initialize Main ArRoutine
    if ((ost_main->routine = RoutineNew(ArRoutineStatus::RUNNABLE)) == nullptr)
        goto ERROR;

    // Init Types
    if (!argon::object::TypesInit())
        goto ERROR;

    // Initialize Main Context
    if ((ost_main->routine->context = ContextNew()) == nullptr)
        goto ERROR;

    return true;

    ERROR:
    if (ost_main != nullptr) {
        if (ost_main->routine != nullptr) {
            if (ost_main->routine->context != nullptr)
                ContextDel(ost_main->routine->context);

            RoutineDel(ost_main->routine);
        }
        FreeOST(ost_main);
        ost_main = nullptr;
    }
    vcores = nullptr;
    argon::memory::FinalizeMemory();
    return false;
}

bool argon::vm::SchedYield(bool resume_last) {
    ArRoutineStatus status = ArRoutineStatus::SUSPENDED;

    if (resume_last)
        status = ArRoutineStatus::BLOCKED;

    if (!CanSpin()) {
        std::this_thread::yield();
        return false;
    }

    GetRoutine()->status = status;
    return true;
}

bool argon::vm::SchedYield(bool resume_last, ArRoutine *routine) {
    ArRoutineStatus status = ArRoutineStatus::SUSPENDED;

    if (resume_last)
        status = ArRoutineStatus::BLOCKED;

    if (!CanSpin()) {
        Spawn(routine);
        std::this_thread::yield();
        return false;
    }

    if (!ost_local->current->queue.EnqueueHead(routine)) {
        routine_gq.EnqueueHead(routine);
        OSTWakeRun();
    }

    GetRoutine()->status = status;
    return true;
}

bool argon::vm::Spawn(ArRoutine *routine) {
    if (!routine_gq.Enqueue(routine))
        return false;

    OSTWakeRun();
    return true;
}

bool argon::vm::Spawns(ArRoutine *routines) {
    ArRoutine *next;

    for (ArRoutine *cursor = routines; cursor != nullptr; cursor = next) {
        next = cursor->next;

        cursor->next = nullptr;
        routine_gq.Enqueue(cursor);
        OSTWakeRun();
    }

    return true;
}

bool argon::vm::Shutdown() {
    short attempt = 10;

    // only the main thread can call shutdown!
    if (ost_main == nullptr)
        return false;

    should_stop = true;
    ost_cond.notify_all();

    while (ost_total > 0 && attempt > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        attempt--;
    }

    if (ost_total == 0) {
        ContextDel(ost_main->routine->context);
        RoutineDel(ost_main->routine);
        FreeOST(ost_main);
        ost_main = nullptr;
        argon::memory::FinalizeMemory();
        return true;
    }

    return false;
}

unsigned long argon::vm::SuspensionReason() {
    ArRoutine *routine = GetRoutine();
    unsigned long reason;

    reason = routine->reason;
    routine->reason = 0;

    return reason;
}

void argon::vm::LockOsThread() {
    if (ost_main == nullptr)
        return;

    ReleaseQueue();
}

void argon::vm::SetVCoreTotal(unsigned int vc) {
    if (vcores != nullptr)
        return;

    vc_total = vc;

    if (vc_total == 0)
        vc_total = std::thread::hardware_concurrency();

    if (vc_total == 0)
        vc_total = VCORE_DEFAULT;
}

void argon::vm::Sleep(unsigned int sec) {
    ReleaseQueue();
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}

void argon::vm::StopTheWorld() {
    std::unique_lock lck(stw_lock);

    if (stw) {
        if (ost_worker_count.fetch_sub(1) == 1)
            cond_stw.notify_one();

        cond_stopwait.wait(lck, [] { return !stw; });
        ost_worker_count++;
    }

    stw = true;

    if (ost_worker_count.fetch_sub(1) > 1)
        cond_stw.wait(lck, [] { return ost_worker_count == 0; });

    assert(ost_worker_count >= 0);
}

void argon::vm::StartTheWorld() {
    stw = false;
    ost_worker_count++;
    cond_stopwait.notify_all();
}

void argon::vm::STWCheckpoint() {
    std::unique_lock lck(stw_lock);

    if (!stw)
        return;

    if (ost_worker_count.fetch_sub(1) == 1)
        cond_stw.notify_one();

    cond_stopwait.wait(lck, [] { return !stw; });
    ost_worker_count++;
}

void argon::vm::ReleaseMain() {
    if (ost_main == nullptr)
        return;

    RoutineReset(GetRoutine(), ArRoutineStatus::RUNNABLE);
    ost_worker_count--;
}

void argon::vm::ReleaseQueue() {
    if (ost_local != nullptr) {
        ReleaseVCore();
        OSTWakeRun();
    }
}

void argon::vm::USleep(unsigned int usec) {
    if (usec > 300000)
        ReleaseQueue();

    std::this_thread::sleep_for(std::chrono::microseconds(usec));
}