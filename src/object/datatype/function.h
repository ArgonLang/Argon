// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_FUNCTION_H_
#define ARGON_OBJECT_FUNCTION_H_

#include <object/arobject.h>
#include "code.h"
#include "namespace.h"
#include "string.h"

#define ARGON_FUNC_NATIVE(cname, name, doc, arity, variadic)        \
ArObject *cname##_fn(Function *self, ArObject **argv);              \
FunctionNative cname = {#name, doc, cname##_fn, arity, variadic};   \
ArObject *cname##_fn(Function *self, ArObject **argv)

namespace argon::object {

    using FunctionNativePtr = ArObject *(*)(struct Function *self, ArObject **argv);

    struct FunctionNative {
        /* Name of native function (this name will be exposed to Argon) */
        const char *name;

        /* Documentation of native function (this doc will be exposed to Argon) */
        const char *doc;

        /* Pointer to native code */
        FunctionNativePtr func;

        /* Arity of the function, how many args accepts in input?! */
        unsigned short arity;

        /* Is a variadic function? (func variadic(p1,p2,...p3))*/
        bool variadic;
    };

    struct Function : ArObject {
        union {
            /* Pointer to Argon code */
            Code *code;

            /* Pointer to structure that contains native code */
            FunctionNativePtr native_fn;
        };

        /* Function name */
        String *name;

        /* List that contains values for partial application */
        List *currying;

        /* List that contains captured variables in a closure */
        List *enclosed;

        /* This pointer points to a structure instance (if and only if this function is a method) */
        ArObject *instance;

        /* Pointer to the global namespace in which this function is declared */
        Namespace *gns;

        /* Arity of the function, how many args accepts in input?! */
        unsigned short arity;

        /* Is a variadic function? (func variadic(p1,p2,...p3)) */
        bool variadic;

        /* This function is native? */
        bool native;

        size_t hash;
    };

    extern const TypeInfo type_function_;

    Function *
    FunctionNew(Namespace *gns, String *name, Code *code, unsigned short arity, bool variadic, List *enclosed);

    Function *FunctionNew(Namespace *gns, const FunctionNative *native);

    Function *FunctionNew(const Function *func, List *currying);

    Function *FunctionNew(const Function *func, ArObject *instance);

} // namespace argon::object

#endif // !ARGON_OBJECT_FUNCTION_H_
