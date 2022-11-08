// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <atomic>
#include <cassert>

#include <vm/datatype/setup.h>

#include "fiber.h"
#include "runtime.h"

using namespace argon::vm;
using namespace argon::vm::datatype;

struct VCore {

};

struct OSThread {

};

thread_local OSThread *ost_local = nullptr;

// Panic management
struct Panic *panic_global = nullptr;
std::atomic<struct Panic *> *panic_oom = nullptr;

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

#define ON_ARGON_CONTEXT if (ost_local != nullptr)

ArObject *argon::vm::GetLastError() {
    ArObject *error;

    ON_ARGON_CONTEXT {
        // TODO: STUB
        assert(false);

        return nullptr;
    }

    if (panic_global == nullptr)
        return nullptr;

    error = IncRef(panic_global->object);

    PanicCleanup(&panic_global);

    return error;
}

bool argon::vm::Initialize() {
    // TODO: STUB
    assert(memory::MemoryInit());
    assert(Setup());

    // TODO: alloc panic_oom

    return true;
}

bool argon::vm::IsPanicking() {
    ON_ARGON_CONTEXT {
        assert(false);
        return true;
    }

    return panic_global != nullptr;
}

void argon::vm::DiscardLastPanic() {
    if (panic_global != nullptr) {
        ON_ARGON_CONTEXT {
            // TODO: STUB
            assert(false);
            return;
        }

        PanicCleanup(&panic_global);
    }
}

void argon::vm::Panic(datatype::ArObject *panic) {
    ON_ARGON_CONTEXT {
        // TODO: STUB
        assert(false);

        return;
    }

    if ((panic_global = PanicNew(panic_global, panic)) == nullptr)
        PanicOOM(&panic_global, panic);
}
