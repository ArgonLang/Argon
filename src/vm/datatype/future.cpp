// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "future.h"

using namespace argon::vm::datatype;

bool future_dtor(Future *self) {
    Release(self->success);
    Release(self->error);

    return true;
}

TypeInfo FutureType = {
        AROBJ_HEAD_INIT_TYPE,
        "Future",
        nullptr,
        nullptr,
        sizeof(Future),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) future_dtor,
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
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_future_ = &FutureType;

ArObject *argon::vm::datatype::FutureResult(Future *future, ArObject *success, ArObject *error) {
    future->error = nullptr;

    if (success != nullptr)
        future->success = IncRef(success);
    else
        future->error = IncRef(error);

    future->wait.cond.notify_one();
}

Future *argon::vm::datatype::FutureNew() {
    auto *future = MakeObject<Future>(type_future_);

    if (future != nullptr) {
        future->success = nullptr;
        future->error = nullptr;

        if ((future->status = AtomNew("Pending")) == nullptr)
            Release(future);

        new(&future->wait.lock)std::mutex();
        new(&future->wait.cond)std::condition_variable();
    }

    return future;
}

void argon::vm::datatype::FutureWait(Future *future) {
    std::unique_lock lock(future->wait.lock);

    future->wait.cond.wait(lock, [future] {
        return future->success != nullptr || future->error != nullptr;
    });
}