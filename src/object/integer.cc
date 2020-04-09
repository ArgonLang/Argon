// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include "integer.h"

using namespace argon::object;

bool integer_equal(ArObject *self, ArObject *other) {
    if (self != other) {
        if (self->type == other->type)
            return ((Integer *) self)->integer == ((Integer *) other)->integer;
        return false;
    }
    return true;
}

size_t integer_hash(ArObject *obj) {
    return ((Integer *) obj)->integer;
}

const NumberActions integer_actions{
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

const TypeInfo type_integer_ = {
        (const unsigned char *) "integer",
        sizeof(Integer),
        &integer_actions,
        nullptr,
        nullptr,
        integer_equal,
        integer_hash,
        nullptr
};

Integer *argon::object::IntegerNew(long number) {
    auto integer = (Integer *) argon::memory::Alloc(sizeof(Integer));
    assert(integer != nullptr);
    integer->strong_or_ref = 1;
    integer->type = &type_integer_;
    integer->integer = number;
    return integer;
}

Integer *argon::object::IntegerNewFromString(const std::string &string, int base) {
    auto integer = (Integer *) argon::memory::Alloc(sizeof(Integer));
    assert(integer != nullptr);
    integer->strong_or_ref = 1;
    integer->type = &type_integer_;
    integer->integer = std::strtol(string.c_str(), nullptr, base);
    return integer;
}
