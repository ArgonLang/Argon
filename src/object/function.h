// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_FUNCTION_H_
#define ARGON_OBJECT_FUNCTION_H_

#include "object.h"
#include "code.h"

namespace argon::object {

    using NativeFuncPtr = ArObject *(*)(struct Function *self, ArObject **argv);

    struct Function : ArObject {
        union {
            Code *code;
            NativeFuncPtr native_fn;
        };
        List *currying;
        List *enclosed;

        // Method
        ArObject *instance;

        unsigned short arity;
        bool variadic;
        bool native;
    };

    extern const TypeInfo type_function_;

    Function *FunctionNew(Code *code, unsigned short arity, bool variadic, List *enclosed);

    Function *FunctionNew(NativeFuncPtr func, unsigned short arity, bool variadic);

    Function *FunctionNew(const Function *func, unsigned short currying_len);

    Function *FunctionNew(const Function *func, ArObject *instance);

} // namespace argon::object

#endif // !ARGON_OBJECT_FUNCTION_H_
