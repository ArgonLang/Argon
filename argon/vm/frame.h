// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_FRAME_H_
#define ARGON_VM_FRAME_H_

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/code.h>
#include <argon/vm/datatype/list.h>
#include <argon/vm/datatype/namespace.h>

namespace argon::vm {
    struct Frame {
        datatype::ArSize fiber_id;

        /// This counter tracks the use of frame. It is used to prevent panicked frames from being released prematurely.
        datatype::ArSize counter;

        /// Previous frame (caller).
        Frame *back;

        /// Pointer to head of deferred stack.
        struct Defer *defer;

        /// Pointer to global namespace.
        datatype::Namespace *globals;

        /// If the invoked function is a method or a static member of a Struct/Trait,
        /// this pointer points to the type in which the function is defined.
        datatype::ArObject *base;

        /// Pointer to the status variable of Function which contains the address of this frame (generator function).
        void **gen_status;

        /// Code being executed in this frame.
        datatype::Code *code;

        /// Pointer to the last executed instruction.
        unsigned char *instr_ptr;

        /// Pointer to the code trap handler for this frame.
        unsigned char *trap_ptr;

        /// Evaluation stack.
        datatype::ArObject **eval_stack;

        /// Locals variables.
        datatype::ArObject **locals;

        /// Sync keys
        datatype::ArObject **sync_keys;

        /// Enclosing scope (If Any).
        datatype::List *enclosed;

        /// Value to be returned at the end of execution of this frame.
        datatype::ArObject *return_value;

        /// At the end of each frame there is allocated space for(in this order): eval_stack + local_variables + sync_keys
        datatype::ArObject *extra[];
    };
} // argon::vm

#endif // !ARGON_VM_FRAME_H_
