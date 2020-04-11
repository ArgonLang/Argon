// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include "integer.h"
#include "error.h"

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

bool integer_istrue(Integer *self) {
    return self->integer > 0;
}

ArObject *integer_add(Integer *self, ArObject *other) {
    if (self->type == other->type)
        return IntegerNew(self->integer + ((Integer *) other)->integer);
    return ReturnError(NotImpl);
}

ArObject *integer_sub(Integer *self, ArObject *other) {
    if (self->type == other->type)
        return IntegerNew(self->integer - ((Integer *) other)->integer);
    return ReturnError(NotImpl);
}

ArObject *integer_mul(Integer *self, ArObject *other) {
    if (self->type == other->type)
        return IntegerNew(self->integer * ((Integer *) other)->integer);
    return ReturnError(NotImpl);
}

ArObject *integer_div(Integer *self, ArObject *other) {
    if (self->type == other->type)
        return IntegerNew(self->integer / ((Integer *) other)->integer);
    return ReturnError(NotImpl);
}

ArObject *integer_mod(Integer *self, ArObject *other) {
    if (self->type == other->type)
        return IntegerNew(self->integer % ((Integer *) other)->integer);

    return ReturnError(NotImpl);
}

ArObject *integer_lsh(Integer *self, ArObject *other) {
    if (self->type == other->type)
        return IntegerNew(self->integer << ((Integer *) other)->integer);
    return ReturnError(NotImpl);
}

ArObject *integer_rsh(Integer *self, ArObject *other) {
    if (self->type == other->type)
        return IntegerNew(self->integer >> ((Integer *) other)->integer);
    return ReturnError(NotImpl);
}

const NumberActions integer_actions{
        (BinaryOp) integer_div,
};

const TypeInfo type_integer_ = {
        (const unsigned char *) "integer",
        sizeof(Integer),
        &integer_actions,
        nullptr,
        nullptr,
        (BoolUnaryOp) integer_istrue,
        integer_equal,
        integer_hash,
        (BinaryOp) integer_add,
        (BinaryOp) integer_sub,
        (BinaryOp) integer_mul,
        (BinaryOp) integer_div,
        (BinaryOp) integer_mod,
        (BinaryOp) integer_lsh,
        (BinaryOp) integer_rsh,
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
