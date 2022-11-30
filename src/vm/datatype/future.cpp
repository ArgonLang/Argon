// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "future.h"

using namespace argon::vm::datatype;

bool future_dtor(Future *self) {
    Release(self->value);

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

ArObject *argon::vm::datatype::FutureResult(Future *future) {
    return IncRef(future->value);
}

bool argon::vm::datatype::FutureAWait(Future *future) {
    if (future->status != FutureStatus::PENDING)
        return true;

    return future->wait.queue.Wait();
}

Future *argon::vm::datatype::FutureNew() {
    auto *future = MakeObject<Future>(type_future_);

    if (future != nullptr) {
        future->value = nullptr;

        new(&future->wait.lock)std::mutex();
        new(&future->wait.cond)std::condition_variable();
        new(&future->wait.queue)sync::NotifyQueue();

        future->status = FutureStatus::PENDING;
    }

    return future;
}

void argon::vm::datatype::FutureSetResult(Future *future, ArObject *success, ArObject *error) {
    if (success != nullptr) {
        future->value = IncRef(success);
        future->status = FutureStatus::FULFILLED;
    } else {
        future->value = IncRef(error);
        future->status = FutureStatus::REJECTED;
    }

    future->wait.queue.NotifyAll();
    future->wait.cond.notify_one();
}

void argon::vm::datatype::FutureWait(Future *future) {
    std::unique_lock lock(future->wait.lock);

    future->wait.cond.wait(lock, [future] {
        return future->status != FutureStatus::PENDING;
    });
}