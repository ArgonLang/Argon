// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/sync/mutex.h>

#include <object/datatype/nil.h>
#include <object/datatype/error.h>

#include "sync.h"

using namespace argon::object;
using namespace argon::module::sync;

struct Mutex : ArObject {
    argon::vm::sync::Mutex mutex;
};

ARGON_FUNCTION5(mutex_, new, "", 0, false) {
    auto *mutex = ArObjectNew<Mutex>(RCType::INLINE, type_mutex_);

    if (mutex != nullptr)
        new(&mutex->mutex) argon::vm::sync::Mutex();

    return mutex;
}

ARGON_METHOD5(mutex_, lock, "Locks mutex."
                            ""
                            "If the lock is already in use two scenarios are possible:"
                            "1) If this is a suspendable routine, it will be suspended and this thread "
                            "move to execute another routine."
                            ""
                            "2) This routine is not suspendable, this thread release it's routine queue(if have one)"
                            "and wait on C++ std::condition_variable."
                            ""
                            "- Returns: nil", 0, false) {
    auto *mutex = (Mutex *) self;

    if (!mutex->mutex.Lock())
        return nullptr;

    return IncRef(NilVal);
}

ARGON_METHOD5(mutex_, unlock, "Unlocks mutex."
                              ""
                              "- Panic RuntimeError: mutex is not locked on entry to Unlock."
                              "- Returns: nil", 0, false) {
    auto *mutex = (Mutex *) self;

    if (!mutex->mutex.Unlock()) {
        ErrorFormat(argon::object::type_runtime_error_, "unlock of unlocked mutex");
        return nullptr;
    }

    return IncRef(NilVal);
}

const NativeFunc mutex_methods[] = {
        mutex_new_,
        mutex_lock_,
        mutex_unlock_,
        ARGON_METHOD_SENTINEL
};

const TypeInfo *mutex_bases[] = {
        type_locker_,
        nullptr
};

const ObjectSlots mutex_obj = {
        mutex_methods,
        nullptr,
        mutex_bases,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

void mutex_cleanup(Mutex *self) {
    self->mutex.~Mutex();
}

bool mutex_istrue(ArObject *self) {
    return true;
}

const TypeInfo MutexType = {
        TYPEINFO_STATIC_INIT,
        "Mutex",
        nullptr,
        sizeof(Mutex),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) mutex_cleanup,
        nullptr,
        nullptr,
        mutex_istrue,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &mutex_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::module::sync::type_mutex_ = &MutexType;
