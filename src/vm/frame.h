// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_FRAME_H_
#define ARGON_VM_FRAME_H_

#include <vm/datatype/arobject.h>
#include <vm/datatype/code.h>
#include <vm/datatype/list.h>
#include <vm/datatype/namespace.h>

#include "defer.h"

namespace argon::vm {
    struct Frame {
        datatype::ArSize fiber_id;

        /// Previous frame (caller).
        Frame *back;

        /// Pointer to head of deferred stack.
        Defer *defer;

        /// Pointer to global namespace.
        datatype::Namespace *globals;

        /// Pointer to instance object (if method).
        datatype::ArObject *instance;

        /// Code being executed in this frame.
        datatype::Code *code;

        /// Pointer to the last executed instruction.
        unsigned char *instr_ptr;

        /// Evaluation stack.
        datatype::ArObject **eval_stack;

        /// Locals variables.
        datatype::ArObject **locals;

        /// Enclosing scope (If Any).
        datatype::List *enclosed;

        /// Value to be returned at the end of execution of this frame.
        datatype::ArObject *return_value;

        /// At the end of each frame there is allocated space for(in this order): eval_stack + local_variables
        datatype::ArObject *extra[];
    };
} // argon::vm

#endif // !ARGON_VM_FRAME_H_
