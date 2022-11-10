// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "future.h"

using namespace argon::vm::datatype;

TypeInfo FutureType = {
        AROBJ_HEAD_INIT_TYPE,
        "Future",
        nullptr,
        nullptr,
        sizeof(Future),
        TypeInfoFlags::BASE,
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
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_future_ = &FutureType;

ArObject *argon::vm::datatype::FutureResult(Future *future) {
    // TODO: implement this
    return nullptr;
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