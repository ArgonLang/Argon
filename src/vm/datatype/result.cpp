// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arstring.h"
#include "boolean.h"

#include "result.h"

using namespace argon::vm::datatype;

ArObject *result_compare(const Result *self, const ArObject *other, CompareMode mode) {
    const auto *o = (const Result *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self == o)
        return BoolToArBool(true);

    if (self->success == o->success)
        return AR_GET_TYPE(self)->compare(self->value, o->value, mode);

    return BoolToArBool(false);
}

ArObject *result_repr(const Result *self) {
    return (ArObject *) StringFormat("<%s -- success: %s, value: %s>", type_result_->name,
                                     self->success ? "Ok" : "Err", AR_TYPE_NAME(self->value));
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
        (CompareOp) result_compare,
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

        memory::TrackIf((ArObject *) result, value);
    }

    return result;
}