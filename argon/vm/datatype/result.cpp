// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/pcheck.h>
#include <argon/vm/datatype/result.h>

#include <argon/vm/datatype/nil.h>

using namespace argon::vm::datatype;

ARGON_FUNCTION(result_Error, Error,
               "Create a new Result object and set the value to Err.\n"
               "\n"
               "- Parameter obj: Object.\n"
               "- Returns: New Result.\n",
               ": obj", false, false) {
    return (ArObject *) ResultNew(*args, false);
}

ARGON_FUNCTION(result_Ok, Ok,
               "Create a new Result object and set the value to Ok.\n"
               "\n"
               "- Parameter obj: Object.\n"
               "- Returns: New Result.\n",
               ": obj", false, false) {
    return (ArObject *) ResultNew(*args, true);
}

ARGON_METHOD(result_err, err,
             "Returns the contained value if it is Err, otherwise it panics.\n"
             "\n"
             "- Returns: Contained object.\n",
             nullptr, false, false) {
    auto *self = (Result *) _self;

    if (self->success) {
        ErrorFormat(kValueError[0], "%s::err() on Ok value", AR_TYPE_NAME(self));
        return nullptr;
    }

    return IncRef(self->value);
}

ARGON_METHOD(result_ok, ok,
             "Returns the contained value if it is Ok, otherwise it panics.\n"
             "\n"
             "- Returns: Contained object.\n",
             nullptr, false, false) {
    auto *self = (Result *) _self;

    if (!self->success) {
        ErrorFormat(kValueError[0], "%s::ok() on a Err value", AR_TYPE_NAME(self));
        return nullptr;
    }

    return IncRef(self->value);
}

const FunctionDef result_methods[] = {
        result_Error,
        result_Ok,

        result_err,
        result_ok,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots result_objslot = {
        result_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

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
                                     self->success ? "true" : "false", AR_TYPE_NAME(self->value));
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
        &result_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_result_ = &ResultType;

Result *argon::vm::datatype::ResultNew(ArObject *value, bool success) {
    auto *result = MakeGCObject<Result>(&ResultType);

    if (result != nullptr) {
        result->value = IncRef(value == nullptr ? (ArObject *) Nil : value);
        result->success = success;

        memory::TrackIf((ArObject *) result, value);
    }

    return result;
}