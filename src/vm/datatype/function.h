// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_FUNCTION_H_
#define ARGON_VM_DATATYPE_FUNCTION_H_

#include <vm/frame.h>

#include "arobject.h"
#include "arstring.h"
#include "code.h"
#include "error.h"
#include "tuple.h"
#include "namespace.h"
#include "pcheck.h"

namespace argon::vm::datatype {
    enum class FunctionFlags : unsigned char {
        NATIVE = 1,
        METHOD = 1u << 1,
        CLOSURE = 1u << 2,
        VARIADIC = 1u << 3,
        KWARGS = 1u << 4,
        GENERATOR = 1u << 5,
        ASYNC = 1u << 6,
        RECOVERABLE = 1u << 7
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

        [[nodiscard]] void *LockAndGetStatus(void *on_address) {
            uintptr_t expected = 0;

            if (this->lock.compare_exchange_strong(expected, (uintptr_t) on_address)) {
                if (this->IsExhausted()) {
                    this->lock = 0;

                    ErrorFormat(kExhaustedGeneratorError[0],
                                kExhaustedGeneratorError[1],
                                ARGON_RAW_STRING(this->qname));

                    return nullptr;
                }

                return this->status;
            }

            return nullptr;
        }

        void Unlock(void *on_address) {
            auto old = this->lock.exchange(0);
            assert(old == 0 || old == (uintptr_t) on_address);
        }
    };

    extern const TypeInfo *type_function_;

    Function *FunctionInitGenerator(Function *func, vm::Frame *frame);

    ArObject *FunctionInvokeNative(Function *func, ArObject **args, ArSize count, bool kwargs);

    Function *FunctionNew(Code *code, String *name, String *qname, Namespace *ns,
                          List *enclosed, unsigned short arity, FunctionFlags flags);

    Function *FunctionNew(const Function *func, ArObject **args, ArSize nargs);

    Function *FunctionNew(const FunctionDef *func, TypeInfo *base, Namespace *ns);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_FUNCTION_H_
