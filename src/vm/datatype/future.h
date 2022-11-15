// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_DATATYPE_FUTURE_H_
#define ARGON_DATATYPE_FUTURE_H_

#include <thread>
#include <condition_variable>

#include "atom.h"
#include "arobject.h"

namespace argon::vm::datatype {
    struct Future {
        AROBJ_HEAD;

        ArObject *success;
        ArObject *error;

        Atom *status;

        struct {
            std::mutex lock;
            std::condition_variable cond;
        } wait;
    };
    extern const TypeInfo *type_future_;

    ArObject *FutureResult(Future *future, ArObject *success, ArObject *error);

    Future *FutureNew();

    void FutureWait(Future *future);
}

#endif // !ARGON_DATATYPE_FUTURE_H_
