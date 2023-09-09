// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/stringformatter.h>
#include <argon/vm/datatype/boolean.h>

#include <argon/vm/datatype/future.h>

using namespace argon::vm::datatype;

ArObject *future_compare(const ArObject *self, const ArObject *other, CompareMode mode) {
    if (mode == CompareMode::EQ)
        return BoolToArBool(self == other);

    return nullptr;
}

ArObject *future_repr(const Future *self) {
    auto status = self->status;

    if (status == FutureStatus::FULFILLED)
        return (ArObject *) StringFormat("<%s -- status: %s, value: %s>", type_future_->name,
                                         "Fulfilled", AR_TYPE_NAME(self->value));

    return (ArObject *) StringFormat("<%s -- status: %s>", type_future_->name,
                                     status == FutureStatus::PENDING ? "Pending" : "Rejected");
}

bool future_dtor(Future *self) {
    Release(self->value);

    self->wait.lock.~mutex();
    self->wait.cond.~condition_variable();
    self->wait.queue.~NotifyQueue();

    return true;
}

bool future_is_true(const Future *self) {
    return self->status == FutureStatus::FULFILLED;
}

void future_trace(Future *self, Void_UnaryOp trace) {
    trace(self->value);
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
        (TraceOp) future_trace,
        nullptr,
        (Bool_UnaryOp) future_is_true,
        future_compare,
        (UnaryConstOp) future_repr,
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

bool argon::vm::datatype::FutureAWait(Future *future) {
    if (future->status != FutureStatus::PENDING)
        return true;

    return future->wait.queue.Wait();
}

Future *argon::vm::datatype::FutureNew() {
    auto *future = MakeGCObject<Future>(type_future_, false);

    if (future != nullptr) {
        future->value = nullptr;

        new(&future->wait.lock)std::mutex();
        new(&future->wait.cond)std::condition_variable();
        new(&future->wait.queue)sync::NotifyQueue();

        future->status = FutureStatus::PENDING;
    }

    return future;
}

Result *argon::vm::datatype::FutureResult(Future *future) {
    return ResultNew(future->value, future->status == FutureStatus::FULFILLED);
}

void argon::vm::datatype::FutureSetResult(Future *future, ArObject *success, ArObject *error) {
    if (success != nullptr) {
        future->value = IncRef(success);
        future->status = FutureStatus::FULFILLED;
    } else {
        future->value = IncRef(error);
        future->status = FutureStatus::REJECTED;
    }

    memory::TrackIf((ArObject *) future, future->value);

    future->wait.queue.NotifyAll();
    future->wait.cond.notify_one();
}

void argon::vm::datatype::FutureWait(Future *future) {
    std::unique_lock lock(future->wait.lock);

    future->wait.cond.wait(lock, [future] {
        return future->status != FutureStatus::PENDING;
    });
}