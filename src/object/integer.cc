// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "integer.h"

using namespace argon::object;

Integer::Integer(long number) : Number(&type_integer_), integer_(number) {}

Integer::Integer(const std::string &number, int base) : Number(&type_integer_) {
    this->integer_ = std::strtol(number.c_str(), nullptr, base);
}

bool Integer::EqualTo(const Object *other) {
    if (this != other) {
        if (this->type == other->type)
            return this->integer_ == ((Integer *) other)->integer_;
        return false;
    }

    return true;
}

size_t Integer::Hash() {
    return this->integer_;
}
