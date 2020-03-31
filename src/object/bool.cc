// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "bool.h"

using namespace argon::object;


Bool::Bool(bool value) : Object(&type_bool_), value_(value) {
    IncStrongRef(this);
}

bool Bool::EqualTo(const Object *other) {
    return other->type == this->type && ((Bool *) other)->value_;
}

size_t Bool::Hash() {
    return this->value_ ? 1 : 0;
}
