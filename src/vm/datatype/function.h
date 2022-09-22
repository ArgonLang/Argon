// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_FUNCTION_H_
#define ARGON_VM_DATATYPE_FUNCTION_H_

#include "arobject.h"
#include "arstring.h"
#include "tuple.h"
#include "namespace.h"

namespace argon::vm::datatype {
    enum class FunctionFlags : unsigned char {
        NATIVE = 1,
        METHOD = 1u << 2,
        CLOSURE = 1u << 3,
        VARIADIC = 1u << 4,
        KWARGS = 1u << 5,
        GENERATOR = 1u << 6,
        ASYNC = 1u << 7
    };
}

ENUMBITMASK_ENABLE(argon::vm::datatype::FunctionFlags);

namespace argon::vm::datatype {
    struct Function {
        AROBJ_HEAD;

        union {
            /// Pointer to Argon code.
            // Code *code;

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

        /// Tuple that contains captured variables in a closure.
        Tuple *enclosed;

        /// This pointer points to TypeInfo of the DataType in which this method was declared.
        TypeInfo *base;

        /// Pointer to the global namespace in which this function is declared.
        Namespace *gns;

        /// Arity of the function, how many args accepts in input?!.
        unsigned short arity;

        /// Flags, see: FunctionInfo.
        FunctionFlags flags;

        ArSize hash;
    };
    extern const TypeInfo *type_function_;

    Function *FunctionNew(const FunctionDef *func, TypeInfo *base);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_FUNCTION_H_
