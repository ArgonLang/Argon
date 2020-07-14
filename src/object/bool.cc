// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include "bool.h"

using namespace argon::object;

bool bool_equal(ArObject *self, ArObject *other) {
    return (other->type == self->type) && ((Bool *) other)->value;
}

size_t bool_hash(ArObject *obj) {
    return ((Bool *) obj)->value ? 1 : 0;
}

bool bool_istrue(Bool *self) {
    return self->value;
}

const TypeInfo type_bool_ = {
        (const unsigned char *) "bool",
        sizeof(Bool),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (BoolUnaryOp) bool_istrue,
        bool_equal,
        nullptr,
        bool_hash,
        nullptr,
        nullptr
};

Bool *BoolNew(bool value) noexcept {
    auto boolean = ArObjectNew<Bool>(RCType::STATIC, &type_bool_);
    assert(boolean != nullptr);
    boolean->value = value;
    return boolean;
}

Bool *argon::object::True = BoolNew(true);
Bool *argon::object::False = BoolNew(false);
