// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <thread>
#include <object/setup.h>
#include <object/datatype/error.h>

#include "routine_queue.h"
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
    std::atomic<struct OSThread *> ost;

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

    std::thread self;
};

// OsThread variables
OSThread *ost_active = nullptr;             // Working OSThread
OSThread *ost_idle = nullptr;               // IDLE OSThread
thread_local OSThread *ost_local = nullptr; // OSThread for actual thread

unsigned int ost_total = 0;                 // OSThread counter
unsigned int ost_idle_count = 0;            // OSThread counter (idle)

bool should_stop = false;                   // If true all thread should be stopped

// VCore variables
VCore *vcores = nullptr;                    // List of instantiated VCore
unsigned int vc_total = 0;                  // Maximum concurrent VCore
std::atomic_uint vc_idle_count = 0;         // IDLE VCore

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

void FreeOST(OSThread *ost) {
    if (ost != nullptr)
        argon::memory::Free(ost);
}

bool InitializeVCores() {
    if ((vcores = (VCore *) argon::memory::Alloc(sizeof(VCore) * vc_total)) == nullptr)
        return false;

    for (unsigned int i = 0; i < vc_total; i++) {
        vcores[i].ost = nullptr;
        new(&((vcores + i)->queue))ArRoutineQueue(ARGON_VM_QUEUE_MAX_ROUTINES);
        vcores[i].status = VCoreStatus::IDLE;
    }

    return true;
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
        result = Eval(ost_local->routine, frame);
        FrameDel(frame);
    }

    if (IsPanicking())
        Release((ArObject **) &result);

    return result;
}

argon::object::ArObject *argon::vm::Call(argon::object::ArObject *callable, int argc, ...) {
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

ArObject *argon::vm::GetLastError() {
    ArObject *err = nullptr;

    if (ost_local->routine->panic != nullptr)
        err = RoutineRecover(ost_local->routine);

    return err;
}

ArObject *argon::vm::Panic(ArObject *obj) {
    RoutineNewPanic(ost_local->routine, obj);
    return nullptr;
}

ArRoutine *argon::vm::GetRoutine() {
    return ost_local->routine;
}

Context *argon::vm::GetContext() {
    return ost_local->routine->context;
}

bool argon::vm::IsPanicking() {
    return ost_local->routine->panic != nullptr;
}

bool argon::vm::Initialize() {
    vc_total = std::thread::hardware_concurrency();

    if (vc_total == 0)
        vc_total = VCORE_DEFAULT;

    vc_idle_count = vc_total;

    // Initialize memory subsystem
    argon::memory::InitializeMemory();

    if (!InitializeVCores())
        goto ERROR;

    // Setup default OSThread
    if ((ost_local = AllocOST()) == nullptr)
        goto ERROR;

    // Initialize Main ArRoutine
    if ((ost_local->routine = RoutineNew(ArRoutineStatus::RUNNABLE)) == nullptr)
        goto ERROR;

    // Init Types
    if (!argon::object::TypesInit())
        goto ERROR;

    // Initialize Main Context
    if ((ost_local->routine->context = ContextNew()) == nullptr)
        goto ERROR;

    return true;

    ERROR:
    if (ost_local != nullptr) {
        if (ost_local->routine != nullptr) {
            if (ost_local->routine->context != nullptr)
                ContextDel(ost_local->routine->context);

            RoutineDel(ost_local->routine);
        }
        FreeOST(ost_local);
        ost_local = nullptr;
    }
    vcores = nullptr;
    argon::memory::FinalizeMemory();
    return false;
}

bool argon::vm::Shutdown() {
    short attempt = 10;

    should_stop = true;

    while (ost_total > 0 && attempt > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        attempt--;
    }

    if (ost_total == 0) {
        ContextDel(ost_local->routine->context);
        RoutineDel(ost_local->routine);
        FreeOST(ost_local);
        ost_local = nullptr;
        argon::memory::FinalizeMemory();
        return true;
    }

    return false;
}