// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <atomic>

#include <vm/runtime.h>
#include <vm/sync/mutex.h>
#include <vm/sync/queue.h>

#include <object/datatype/nil.h>
#include <object/datatype/error.h>

#include "sync.h"

using namespace argon::object;
using namespace argon::module::sync;

#define MAXREADERS (1<<30)

struct RWMutex : ArObject {
    argon::vm::sync::Mutex w;
    argon::vm::sync::Queue wq;
    argon::vm::sync::Queue rq;

    std::atomic_int reader_count;
    std::atomic_int reader_wait;
};

ARGON_FUNCTION5(rwmutex_, new, "", 0, false) {
    auto *rw = ArObjectNew<RWMutex>(RCType::INLINE, type_mutex_);

    if (rw != nullptr) {
        new(&rw->w) argon::vm::sync::Mutex();
        new(&rw->wq) argon::vm::sync::Queue();
        new(&rw->rq) argon::vm::sync::Queue();

        rw->reader_count = 0;
        rw->reader_wait = 0;
    }

    return rw;
}

ARGON_METHOD5(rwmutex_, lock, "Locks mutex."
                              ""
                              "If the lock is already in use two scenarios are possible:"
                              "1) If this is a suspendable routine, it will be suspended and this thread "
                              "move to execute another routine."
                              ""
                              "2) This routine is not suspendable, this thread release it's routine queue_(if have one)"
                              "and wait on C++ std::condition_variable."
                              ""
                              "- Returns: nil", 0, false) {
    auto *rw = (RWMutex *) self;
    bool wait = false;

    int rcount;

    if (argon::vm::SuspensionReason() == 0) {
        if (!rw->w.Lock())
            return nullptr;

        rcount = rw->reader_count.fetch_add(-MAXREADERS);
        wait = rcount != 0 && rw->reader_wait.fetch_add(rcount) != 0;
    }

    if (wait && rw->wq.Enqueue(true, 1))
        return nullptr;

    return IncRef(NilVal);
}

ARGON_METHOD5(rwmutex_, rlock, "Locks mutex for reading."
                               ""
                               "- Returns: nil", 0, false) {
    auto *rw = (RWMutex *) self;

    if (rw->reader_count.fetch_add(1) < 0)
        rw->rq.Enqueue(false, 0);

    return IncRef(NilVal);
}

ARGON_METHOD5(rwmutex_, unlock, "Unlocks mutex."
                                ""
                                "- Panic RuntimeError: mutex is not locked on entry to Unlock."
                                "- Returns: nil", 0, false) {

    auto *rw = (RWMutex *) self;

    // Announce to readers there is no active writer.
    if ((rw->reader_count.fetch_add(MAXREADERS) + MAXREADERS) >= MAXREADERS)
        return ErrorFormat(argon::object::type_runtime_error_, "unlock of unlocked mutex");

    // Unblock readers
    rw->rq.Broadcast();

    // Allow other writers
    rw->w.Unlock();
    return nullptr;
}

ARGON_METHOD5(rwmutex_, runlock, "Undoes a rlock call."
                                 ""
                                 "- Panic RuntimeError: mutex is not locked on entry to runlock."
                                 "- Returns: nil", 0, false) {
    auto *rw = (RWMutex *) self;
    int rcount;

    if ((rcount = rw->reader_count.fetch_sub(1)) < 0) {
        // Unlock slow
        if (rcount + 1 == 0 || rcount + 1 == -MAXREADERS)
            return ErrorFormat(argon::object::type_runtime_error_, "unlock of unlocked mutex");

        if (rw->reader_wait.fetch_sub(1) == 1)
            rw->wq.Notify();
    }

    return IncRef(NilVal);
}

const NativeFunc rwmutex_methods[] = {
        rwmutex_new_,
        rwmutex_lock_,
        rwmutex_unlock_,
        rwmutex_rlock_,
        rwmutex_runlock_,
        ARGON_METHOD_SENTINEL
};

const TypeInfo *rwmutex_bases[] = {
        type_locker_,
        nullptr
};

const ObjectSlots rwmutex_obj = {
        rwmutex_methods,
        nullptr,
        rwmutex_bases,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

void rwmutex_cleanup(RWMutex *self) {
    self->w.~Mutex();
    self->wq.~Queue();
    self->rq.~Queue();
}

bool rwmutex_istrue(ArObject *self) {
    return true;
}

const TypeInfo RWMutexType = {
        TYPEINFO_STATIC_INIT,
        "RWMutex",
        nullptr,
        sizeof(RWMutex),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) rwmutex_cleanup,
        nullptr,
        nullptr,
        rwmutex_istrue,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &rwmutex_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::module::sync::type_rwmutex_ = &RWMutexType;
