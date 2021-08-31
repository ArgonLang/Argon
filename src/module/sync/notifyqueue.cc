// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/sync/queue.h>

#include <object/datatype/integer.h>
#include <object/datatype/nil.h>
#include <object/datatype/error.h>

#include "sync.h"

using namespace argon::object;
using namespace argon::module::sync;

struct NotifyQueue : ArObject {
    argon::vm::sync::Queue queue;
};

ARGON_FUNCTION5(nq_, new, "", 0, false) {
    auto *nq = ArObjectNew<NotifyQueue>(RCType::INLINE, type_notifyqueue_);

    if (nq != nullptr)
        new(&nq->queue) argon::vm::sync::Queue();

    return nq;
}

ARGON_METHOD5(nq_, getticket, "Returns a ticket for the queue."
                              ""
                              "- Returns: Integer that represent a ticket.", 0, false) {
    auto *nq = (NotifyQueue *) self;
    return IntegerNew(nq->queue.GetTicket());
}

ARGON_METHOD5(nq_, wait, "Wait in the queue."
                         ""
                         "- Parameter ticket: ticket."
                         "- Returns: nil", 1, false) {
    auto *nq = (NotifyQueue *) self;
    IntegerUnderlying ticket;

    if (!AR_TYPEOF(*argv, type_integer_))
        return ErrorFormat(type_type_error_, "expected ticket as integer, got '%s'", AR_TYPE_NAME(*argv));

    ticket = ((Integer *) argv[0])->integer;

    nq->queue.Enqueue(false, 0, ticket);

    return IncRef(NilVal);
}

ARGON_METHOD5(nq_, notify, "Wakes next routine in Queue."
                           ""
                           "- Returns: nil", 0, false) {
    auto *nq = (NotifyQueue *) self;

    nq->queue.Notify();

    return IncRef(NilVal);
}

ARGON_METHOD5(nq_, notifyall, "Wakes all routine waiting on Queue."
                              ""
                              "- Returns: nil", 0, false) {
    auto *nq = (NotifyQueue *) self;

    nq->queue.Broadcast();

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
    self->queue.~Queue();
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
