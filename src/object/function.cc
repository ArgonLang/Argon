// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "function.h"

using namespace argon::object;
using namespace argon::memory;

const TypeInfo argon::object::type_function_ = {
        (const unsigned char *) "function",
        sizeof(Function),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
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

Function *argon::object::FunctionNew(Code *code, unsigned short arity) {
    auto fn = (Function *) Alloc(sizeof(Function));
    fn->strong_or_ref = 1;
    fn->type = &type_function_;

    IncRef(code);
    fn->code = code;
    fn->arity = arity;

    return fn;
}