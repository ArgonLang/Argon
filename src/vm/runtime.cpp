// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <atomic>
#include <cassert>

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
};

struct OSThread {
    OSThread *next;
    OSThread **prev;

    Fiber *fiber;

    std::thread self;
};

// OsThread variables
OSThread *ost_active = nullptr;             // Working OSThread
OSThread *ost_idle = nullptr;               // IDLE OSThread
thread_local OSThread *ost_local = nullptr; // OSThread for actual thread

// VCore variables
VCore *vcores = nullptr;                    // List of instantiated VCore
unsigned int vc_total = 0;                  // Maximum concurrent VCore

// Panic management
struct Panic *panic_global = nullptr;
std::atomic<struct Panic *> *panic_oom = nullptr;

// Global queues
FiberQueue fiber_global;

// Internal

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

OSThread *AllocOST() {
    auto *ost = (OSThread *) memory::Calloc(sizeof(OSThread));

    if (ost != nullptr)
        new(&ost->self) std::thread();

    return ost;
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

bool argon::vm::Initialize(const Config *config) {
    if (!memory::MemoryInit())
        return false;

    if (!InitializeVCores(config->max_vc)) {
        memory::MemoryFinalize();
        return false;
    }

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
