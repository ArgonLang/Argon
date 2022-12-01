// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arstring.h"
#include "result.h"

using namespace argon::vm::datatype;

ArObject *result_repr(const Result *self) {
    return (ArObject *) StringFormat("%s<%s, %s>", type_result_->name,
                                     AR_TYPE_NAME(self->value), self->success ? "Ok" : "Err");
}

bool result_dtor(Result *self) {
    Release(self->value);

    return true;
}

bool result_is_true(const Result *self) {
    return self->success;
}

void result_trace(Result *self, Void_UnaryOp trace) {
    trace(self->value);
}

TypeInfo ResultType = {
        AROBJ_HEAD_INIT_TYPE,
        "Result",
        nullptr,
        nullptr,
        sizeof(Result),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) result_dtor,
        (TraceOp) result_trace,
        nullptr,
        (Bool_UnaryOp) result_is_true,
        nullptr,
        (UnaryConstOp) result_repr,
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
const TypeInfo *argon::vm::datatype::type_result_ = &ResultType;

Result *argon::vm::datatype::ResultNew(ArObject *value, bool success) {
    auto *result = MakeGCObject<Result>(&ResultType, false);

    if (result != nullptr) {
        result->value = IncRef(value);
        result->success = success;

        // TODO: track
    }

    return result;
}