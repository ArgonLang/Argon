// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_FUNCTION_H_
#define ARGON_VM_DATATYPE_FUNCTION_H_

#include <argon/vm/frame.h>

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/code.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/tuple.h>
#include <argon/vm/datatype/namespace.h>
#include <argon/vm/datatype/pcheck.h>

namespace argon::vm::datatype {
    enum class FunctionFlags : unsigned short {
        ASYNC = 1,
        DEFARGS = 1u << 1,
        GENERATOR = 1u << 2,
        KWARGS = 1u << 3,
        METHOD = 1u << 4,
        STATIC = 1u << 5,
        VARIADIC = 1U << 6,

        // Not usable at compile time
        NATIVE = 1u << 7,
        RECOVERABLE = 1u << 8
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

        /// Params checker
        PCheck *pcheck;

        /// Tuple that contains values for partial application.
        Tuple *currying;

        /// Tuple that contains default arguments for named parameters (e.g.: a=1,b=2,c=)
        Tuple *default_args;

        /// List that contains captured variables in a closure.
        List *enclosed;

        /// This pointer points to TypeInfo of the DataType in which this method was declared.
        TypeInfo *base;

        /// Pointer to the global namespace in which this function is declared.
        Namespace *gns;

        /// Pointer to status object(e.g. vm::Frame) valid only if it is a generator and recoverable function.
        void *status;

        /// Prevents another thread from executing this generator at the same time.
        std::atomic_uintptr_t lock;

        /// Arity of the function, how many args accepts in input?!.
        unsigned short arity;

        /// Flags, see: FunctionInfo.
        FunctionFlags flags;

        [[nodiscard]] bool HaveDefaults() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::DEFARGS);
        }

        [[nodiscard]] bool IsAsync() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::ASYNC);
        }

        [[nodiscard]] bool IsExhausted() const {
            return this->status == nullptr && ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::RECOVERABLE);
        }

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

        [[nodiscard]] bool IsRecoverable() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::RECOVERABLE);
        }

        [[nodiscard]] bool IsVariadic() const {
            return ENUMBITMASK_ISTRUE(this->flags, FunctionFlags::VARIADIC);
        }

        [[nodiscard]] void *LockAndGetStatus(void *on_address);

        void Unlock(void *on_address) {
            auto old = this->lock.exchange(0);
            assert(old == 0 || old == (uintptr_t) on_address);
        }
    };

    _ARGONAPI extern const TypeInfo *type_function_;

    Function *FunctionInitGenerator(Function *func, vm::Frame *frame);

    ArObject *FunctionInvokeNative(Function *func, ArObject **args, ArSize count, bool kwargs);

    Function *FunctionNew(Code *code, Namespace *ns, Tuple *default_args, List *enclosed,
                          unsigned short arity, FunctionFlags flags);

    Function *FunctionNew(const Function *func, ArObject **args, ArSize nargs);

    Function *FunctionNew(const FunctionDef *func, TypeInfo *base, Namespace *ns);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_FUNCTION_H_
