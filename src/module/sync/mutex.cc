// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <atomic>
#include <condition_variable>

#include <vm/runtime.h>

#include <object/datatype/nil.h>
#include <object/datatype/error.h>

#include "sync.h"

using namespace argon::object;
using namespace argon::module::sync;

enum class MutexStatus {
    UNLOCKED,
    LOCKED
};

struct Mutex : ArObject {
    std::atomic<MutexStatus> flags;
    argon::vm::ArRoutineQueue queue;

    std::mutex os_lock;
    std::condition_variable os_cond;
    ArSize blocked;
};

ARGON_FUNCTION5(mutex_, new, "", 0, false) {
    auto *mutex = ArObjectNew<Mutex>(RCType::INLINE, type_mutex_);

    if (mutex != nullptr) {
        mutex->flags = MutexStatus::UNLOCKED;

        new(&mutex->queue) argon::vm::ArRoutineQueue();
        new(&mutex->os_lock) std::mutex();
        new(&mutex->os_cond) std::condition_variable();

        mutex->blocked = 0;
    }

    return mutex;
}

ArObject *lock_slow(Mutex *self) {
    MutexStatus expected = MutexStatus::UNLOCKED;
    bool spin = argon::vm::CanSpin();

    std::unique_lock lock(self->os_lock);

    if (self->flags.compare_exchange_strong(expected, MutexStatus::LOCKED))
        return IncRef(NilVal);

    if (spin) {
        self->queue.Enqueue(argon::vm::UnschedRoutine(true));
        return nullptr;
    }

    argon::vm::ReleaseQueue();

    self->blocked++;
    self->os_cond.wait(lock, [self, &expected]{
        expected = MutexStatus::UNLOCKED;
        return self->flags.compare_exchange_strong(expected, MutexStatus::LOCKED);
    });
    self->blocked--;

    return IncRef(NilVal);
}

ArObject *unlock_slow(Mutex *self) {
    std::unique_lock lock(self->os_lock);
    argon::vm::ArRoutine *routine;

    if (self->blocked == 0) {
        routine = self->queue.Dequeue();

        if (routine != nullptr)
            argon::vm::SchedYield(false, routine);
    } else
        self->os_cond.notify_one();

    return IncRef(NilVal);
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
    MutexStatus expected = MutexStatus::UNLOCKED;
    auto *mutex = (Mutex *) self;

    if (mutex->flags.compare_exchange_strong(expected, MutexStatus::LOCKED))
        return IncRef(NilVal);

    return lock_slow(mutex);
}

ARGON_METHOD5(mutex_, unlock, "Unlocks mutex."
                              ""
                              "- Panic RuntimeError: mutex is not locked on entry to Unlock."
                              "- Returns: nil", 0, false) {
    MutexStatus expected = MutexStatus::LOCKED;
    auto *mutex = (Mutex *) self;

    if (!mutex->flags.compare_exchange_strong(expected, MutexStatus::UNLOCKED))
        return ErrorFormat(type_runtime_error_, "unlock of unlocked mutex");

    return unlock_slow(mutex);
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
    self->os_cond.~condition_variable();
    self->os_lock.~mutex();
    self->queue.~ArRoutineQueue();
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
        &mutex_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::module::sync::type_mutex_ = &MutexType;
