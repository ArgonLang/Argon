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

const TypeInfo type_bool_ = {
        (const unsigned char *) "bool",
        sizeof(Bool),
        nullptr,
        nullptr,
        nullptr,
        bool_equal,
        bool_hash,
        nullptr
};

Bool *BoolNew(bool value) noexcept {
    auto boolean = (Bool *) argon::memory::Alloc(sizeof(Bool));
    assert(boolean != nullptr);
    boolean->strong_or_ref = 1;
    boolean->type = &type_bool_;
    boolean->value = value;
    return boolean;
}

Bool *argon::object::True = BoolNew(true);
Bool *argon::object::False = BoolNew(false);
