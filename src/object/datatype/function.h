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
    enum class FunctionFlags : unsigned char {
        NATIVE = 1,
        METHOD = 1u << 2,
        CLOSURE = 1u << 3,
        VARIADIC = 1u << 4,
        GENERATOR = 1u << 5
    };
}

ENUMBITMASK_ENABLE(argon::object::FunctionFlags);

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

        /* Function qualified name */
        String *qname;

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

        /* Pointer to status object (if this function is a Generator) */
        ArObject *status;

        /* Arity of the function, how many args accepts in input?! */
        unsigned short arity;

        /* Flags, see: FunctionInfo */
        FunctionFlags flags;

        ArSize hash;

        [[nodiscard]] bool IsNative() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::NATIVE);
        }

        [[nodiscard]] bool IsMethod() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::METHOD);
        }

        [[nodiscard]] bool IsClosure() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::CLOSURE);
        }

        [[nodiscard]] bool IsVariadic() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::VARIADIC);
        }

        [[nodiscard]] bool IsGenerator() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::GENERATOR);
        }

        [[nodiscard]] bool IsRecoverable() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::GENERATOR) && this->status != nullptr;
        }

        [[nodiscard]] ArObject *GetStatus() const {
            return IncRef(this->status);
        };
    };

    extern const TypeInfo *type_function_;

    Function *FunctionNew(Namespace *gns, String *name, String *doc, Code *code,
                          List *enclosed, unsigned short arity, FunctionFlags flags);

    Function *FunctionNew(Namespace *gns, TypeInfo *base, const NativeFunc *native, bool method);

    Function *FunctionNew(const Function *func, List *currying);

    Function *FunctionNewStatus(const Function *func, ArObject *status);

    ArObject *FunctionCallNative(Function *func, ArObject **args, ArSize count);

} // namespace argon::object


#endif // !ARGON_OBJECT_FUNCTION_H_
