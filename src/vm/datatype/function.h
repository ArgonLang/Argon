// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_FUNCTION_H_
#define ARGON_VM_DATATYPE_FUNCTION_H_

#include "arobject.h"
#include "arstring.h"
#include "code.h"
#include "tuple.h"
#include "namespace.h"

namespace argon::vm::datatype {
    enum class FunctionFlags : unsigned char {
        NATIVE = 1,
        METHOD = 1u << 1,
        CLOSURE = 1u << 2,
        VARIADIC = 1u << 3,
        KWARGS = 1u << 4,
        GENERATOR = 1u << 5,
        ASYNC = 1u << 6
    };
}

ENUMBITMASK_ENABLE(argon::vm::datatype::FunctionFlags);

namespace argon::vm::datatype {
    struct Function {
        AROBJ_HEAD;

        union {
            /// Pointer to Argon code.
            Code *code;

            /// Pointer to native code.
            FunctionPtr native;
        };

        /// Function name.
        String *name;

        /// Function qualified name.
        String *qname;

        /// Function docs.
        String *doc;

        /// Tuple that contains values for partial application.
        Tuple *currying;

        /// List that contains captured variables in a closure.
        List *enclosed;

        /// This pointer points to TypeInfo of the DataType in which this method was declared.
        TypeInfo *base;

        /// Pointer to the global namespace in which this function is declared.
        Namespace *gns;

        /// Arity of the function, how many args accepts in input?!.
        unsigned short arity;

        /// Flags, see: FunctionInfo.
        FunctionFlags flags;

        ArSize hash;

        [[nodiscard]] bool IsGenerator() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::GENERATOR);
        }

        [[nodiscard]] bool IsKWArgs() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::KWARGS);
        }

        [[nodiscard]] bool IsMethod() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::METHOD);
        }

        [[nodiscard]] bool IsNative() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::NATIVE);
        }

        [[nodiscard]] bool IsVariadic() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::VARIADIC);
        }
    };
    extern const TypeInfo *type_function_;

    ArObject *FunctionInvokeNative(Function *func, ArObject **args, ArSize count, bool kwargs);

    Function *FunctionNew(Code *code, String *name, String *qname, Namespace *ns,
                          List *enclosed, unsigned short arity, FunctionFlags flags);

    Function *FunctionNew(const Function *func, ArObject **args, ArSize nargs);

    Function *FunctionNew(const FunctionDef *func, TypeInfo *base, Namespace *ns);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_FUNCTION_H_
