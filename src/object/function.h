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
        argon::object::List *currying;
        argon::object::List *enclosed;

        unsigned short arity;
        bool variadic;
    };

    extern const TypeInfo type_function_;

    Function *FunctionNew(argon::object::Code *code, unsigned short arity, bool variadic);

    Function *FunctionNew(const Function *func, unsigned short currying_len);

    Function *FunctionNew(const Function *func, argon::object::List *enclosed);

} // namespace argon::object

#endif // !ARGON_OBJECT_FUNCTION_H_
