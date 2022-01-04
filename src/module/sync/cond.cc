// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/sync/queue.h>

#include <object/datatype/nil.h>
#include <object/datatype/error.h>

#include "sync.h"

using namespace argon::object;
using namespace argon::module::sync;

struct Cond : ArObject {
    argon::vm::sync::Queue queue;
    ArObject *lock;
};

ARGON_FUNCTION5(cond_, new, "Returns new Cond."
                            ""
                            "- Parameter lock: An object that implements Locker trait."
                            "- Returns: Cond object."
                            "- Panic TypeError: object not implement Locker trait.", 1, false) {
    Cond *cond;

    if (!TraitIsImplemented(*argv, type_locker_))
        return ErrorFormat(type_type_error_,"'%s' does not implement the trait Locker", AR_TYPE_NAME(*argv));

    if ((cond = ArObjectNew<Cond>(RCType::INLINE, type_cond_)) != nullptr) {
        new(&cond->queue) argon::vm::sync::Queue();
        cond->lock = IncRef(argv[0]);
    }

    return cond;
}

ARGON_METHOD5(cond_, wait, "Atomically unlocks Cond.Lock and suspends execution of the calling routine."
                           ""
                           "After resuming execution, Wait locks Cond.Lock before returning."
                           "Unlike in other systems, Wait cannot return unless awoken by Broadcast or Signal."
                           ""
                           "- Returns: nil", 0, false) {
    auto *cond = (Cond *) self;
    ArObject *args[1] = {
            cond->lock
    };

    unsigned int ticket;

    if (argon::vm::SuspensionReason() == 0) {
        ticket = cond->queue.GetTicket();

        argon::vm::CallMethod(cond->lock, "unlock", 1, args);

        if (cond->queue.Enqueue(true, 1, ticket))
            return nullptr;
    }

    if (argon::vm::CallMethod(cond->lock, "lock", 1, args) == nullptr) {
        argon::vm::GetRoutine()->reason = 1;
        return nullptr;
    }

    return IncRef(NilVal);
}

ARGON_METHOD5(cond_, signal, "Wakes one routine waiting on Cond."
                             ""
                             "- Returns: nil", 0, false) {
    auto *cond = (Cond *) self;

    cond->queue.Notify();

    return IncRef(NilVal);
}

ARGON_METHOD5(cond_, broadcast, "Wakes all routine waiting on Cond."
                                ""
                                "- Returns: nil", 0, false) {
    auto *cond = (Cond *) self;

    cond->queue.Broadcast();

    return IncRef(NilVal);
}

const NativeFunc cond_methods[] = {
        cond_new_,
        cond_wait_,
        cond_signal_,
        cond_broadcast_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots cond_obj = {
        cond_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

void cond_cleanup(Cond *self) {
    Release(self->lock);
    self->queue.~Queue();
}

bool cond_istrue(ArObject *self) {
    return true;
}

const TypeInfo CondType = {
        TYPEINFO_STATIC_INIT,
        "Cond",
        nullptr,
        sizeof(Cond),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) cond_cleanup,
        nullptr,
        nullptr,
        cond_istrue,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &cond_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::module::sync::type_cond_ = &CondType;
