// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_FRAME_H_
#define ARGON_VM_FRAME_H_

#include <object/code.h>

namespace argon::vm {
    struct Frame {
        /* Previous frame (caller) */
        Frame *back;

        /* Pointer to global namespace */
        object::ArObject *globals;

        /* Code being executed in this frame */
        object::Code *code;

        /* Pointer to the last executed instruction */
        unsigned char *instr_ptr;

        /* Evaluation stack */
        object::ArObject **eval_stack;

        /* Locals variables */
        object::ArObject **locals;

        /* Enclosing scope (If Any) */
        object::ArObject **enclosed;

        /* At the end of each frame there is allocated space for(in this order): eval_stack + local_variables */
        unsigned char *stack_extra_base[];
    };

    Frame *FrameNew(object::Code *code);

    void FrameDel(Frame *frame);

} // namespace argon::vm

#endif // !ARGON_VM_FRAME_H_
