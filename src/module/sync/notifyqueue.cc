// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <condition_variable>

#include <vm/runtime.h>
#include <vm/notifyqueue.h>

#include <object/datatype/integer.h>
#include <object/datatype/nil.h>
#include <object/datatype/error.h>

#include "sync.h"

using namespace argon::object;
using namespace argon::module::sync;

struct NotifyQueue : ArObject {
    argon::vm::ArRoutineNotifyQueue queue;

    std::mutex os_lock;
    std::condition_variable os_cond;
};

ARGON_FUNCTION5(nq_, new, "", 0, false) {
    auto *nq = ArObjectNew<NotifyQueue>(RCType::INLINE, type_notifyqueue_);

    if (nq != nullptr) {
        new(&nq->queue) argon::vm::ArRoutineNotifyQueue();
        new(&nq->os_lock) std::mutex();
        new(&nq->os_cond) std::condition_variable();
    }

    return nq;
}

ARGON_METHOD5(nq_, getticket, "", 0, false) {
    auto *nq = (NotifyQueue *) self;
    return IntegerNew(nq->queue.GetTicket());
}

ARGON_METHOD5(nq_, wait, "", 1, false) {
    auto *nq = (NotifyQueue *) self;
    IntegerUnderlying ticket;

    if (!AR_TYPEOF(*argv, type_integer_))
        return ErrorFormat(type_type_error_, "expected ticket as integer, got '%s'", AR_TYPE_NAME(*argv));

    ticket = ((Integer *) argv[0])->integer;

    if (argon::vm::CanSpin()) {
        if (nq->queue.Wait(argon::vm::GetRoutine(), ticket))
            argon::vm::UnschedRoutine(false);
    } else {
        argon::vm::ReleaseQueue();

        std::unique_lock lock(nq->os_lock);
        nq->os_cond.wait(lock, [nq, ticket] {
            return nq->queue.IsTicketExpired(ticket);
        });
    }

    return IncRef(NilVal);
}

ARGON_METHOD5(nq_, notify, "", 0, false) {
    auto *nq = (NotifyQueue *) self;
    argon::vm::ArRoutine *routine;

    nq->os_cond.notify_one();

    if ((routine = nq->queue.Notify()) != nullptr)
        argon::vm::SchedYield(false, routine);

    return IncRef(NilVal);
}

ARGON_METHOD5(nq_, notifyall, "", 0, false) {
    auto *nq = (NotifyQueue *) self;
    argon::vm::ArRoutine *routines;

    nq->os_cond.notify_all();

    routines = nq->queue.NotifyAll();
    argon::vm::Spawns(routines);

    return IncRef(NilVal);
}

const NativeFunc nq_methods[] = {
        nq_getticket_,
        nq_notify_,
        nq_notifyall_,
        nq_wait_,
        nq_new_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots nq_obj = {
        nq_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

void nq_cleanup(NotifyQueue *self) {
    self->queue.~ArRoutineNotifyQueue();
    self->os_cond.~condition_variable();
    self->os_lock.~mutex();
}

const TypeInfo NotifyQueueType = {
        TYPEINFO_STATIC_INIT,
        "NotifyQueue",
        nullptr,
        sizeof(NotifyQueue),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp)nq_cleanup,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &nq_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::module::sync::type_notifyqueue_ = &NotifyQueueType;
