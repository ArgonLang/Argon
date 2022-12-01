// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_FUTURE_H_
#define ARGON_VM_DATATYPE_FUTURE_H_

#include <thread>
#include <condition_variable>

#include <vm/sync/ticket.h>

#include "result.h"
#include "arobject.h"

namespace argon::vm::datatype {
    enum class FutureStatus {
        FULFILLED,
        PENDING,
        REJECTED
    };

    struct Future {
        AROBJ_HEAD;

        ArObject *value;

        struct {
            std::mutex lock;
            std::condition_variable cond;

            sync::NotifyQueue queue;
        } wait;

        FutureStatus status;
    };
    extern const TypeInfo *type_future_;

    bool FutureAWait(Future *future);

    Future *FutureNew();

    Result *FutureResult(Future *future);

    void FutureSetResult(Future *future, ArObject *success, ArObject *error);

    void FutureWait(Future *future);
}

#endif // !ARGON_VM_DATATYPE_FUTURE_H_
