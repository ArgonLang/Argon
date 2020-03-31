// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "nil.h"

using namespace argon::object;

Nil::Nil() : Object(&type_nil_) {
    IncStrongRef(this);
}

bool Nil::EqualTo(const Object *other) {
    return other->type == this->type;
}

size_t Nil::Hash() {
    return 0;
}
