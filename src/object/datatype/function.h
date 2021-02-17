// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_FUNCTION_H_
#define ARGON_OBJECT_FUNCTION_H_

#include <utils/enum_bitmask.h>

#include <object/arobject.h>
#include "code.h"
#include "namespace.h"
#include "string.h"

namespace argon::object {
    enum class FunctionType : unsigned char {
        NATIVE = 1,
        METHOD = 1u << 2,
        CLOSURE = 1u << 3,
        VARIADIC = 1u << 4
    };
}

ENUMBITMASK_ENABLE(argon::object::FunctionType);

namespace argon::object {
    struct Function : ArObject {
        union {
            /* Pointer to Argon code */
            Code *code;

            /* Pointer to structure that contains native code */
            NativeFuncPtr native_fn;
        };

        /* Function name */
        String *name;

        /* Function docs */
        String *doc;

        /* List that contains values for partial application */
        List *currying;

        /* List that contains captured variables in a closure */
        List *enclosed;

        /* This pointer points to TypeInfo of the DataType in which this method was declared */
        TypeInfo *base;

        /* Pointer to the global namespace in which this function is declared */
        Namespace *gns;

        /* Arity of the function, how many args accepts in input?! */
        unsigned short arity;

        /* Flags, see: FunctionInfo */
        FunctionType flags;

        size_t hash;

        [[nodiscard]] bool IsNative() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionType::NATIVE);
        }

        [[nodiscard]] bool IsMethod() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionType::METHOD);
        }

        [[nodiscard]] bool IsClosure() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionType::CLOSURE);
        }

        [[nodiscard]] bool IsVariadic() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionType::VARIADIC);
        }
    };

    extern const TypeInfo type_function_;

    Function *FunctionNew(Namespace *gns, String *name, String *doc, Code *code, List *enclosed, unsigned short arity,
                          FunctionType flags);

    Function *FunctionNew(Namespace *gns, const NativeFunc *native);

    Function *FunctionMethodNew(Namespace *gns, TypeInfo *type, const NativeFunc *native);

    Function *FunctionNew(const Function *func, List *currying);

} // namespace argon::object


#endif // !ARGON_OBJECT_FUNCTION_H_
