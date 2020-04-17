// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_FUNCTION_H_
#define ARGON_OBJECT_FUNCTION_H_

#include "object.h"
#include "code.h"

namespace argon::object {

    struct Function : ArObject {
        argon::object::Code *code;
        unsigned short arity;
        bool variadic;
    };

    extern const TypeInfo type_function_;

    Function *FunctionNew(argon::object::Code *code, unsigned short arity);

} // namespace argon::object

#endif // !ARGON_OBJECT_FUNCTION_H_
