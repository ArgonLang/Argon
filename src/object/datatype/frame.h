// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_FRAME_H_
#define ARGON_OBJECT_FRAME_H_

#include <object/datatype/code.h>
#include <object/datatype/function.h>
#include <object/datatype/list.h>
#include <object/datatype/namespace.h>

#include <object/arobject.h>

namespace argon::object {
    enum class FrameFlags {
        CLEAR = 0,
        MAIN = 1
    };

    struct Frame : ArObject {
        FrameFlags flags;

        /* Previous frame (caller) */
        Frame *back;

        /* Pointer to global namespace */
        object::Namespace *globals;

        /* Pointer to proxy global namespace (an isolated global environment) */
        object::Namespace *proxy_globals;

        /* Pointer to instance object (if method) */
        object::ArObject *instance;

        /* Code being executed in this frame */
        object::Code *code;

        /* Pointer to the last executed instruction */
        unsigned char *instr_ptr;

        /* Evaluation stack */
        object::ArObject **eval_stack;

        /* Locals variables */
        object::ArObject **locals;

        /* Enclosing scope (If Any) */
        object::List *enclosed;

        /* Value to be returned at the end of execution of this frame */
        object::ArObject *return_value;

        /* At the end of each frame there is allocated space for(in this order): eval_stack + local_variables */
        object::ArObject *stack_extra_base[];

        [[nodiscard]] bool IsMain() {
            return this->flags == FrameFlags::MAIN;
        }

        void SetMain() {
            this->flags = FrameFlags::MAIN;
        }

        void UnsetMain() {
            this->flags = FrameFlags::CLEAR;
        }
    };

    extern const TypeInfo *type_frame_;

    Frame *FrameNew(Code *code, Namespace *globals, Namespace *proxy);

    void FrameFill(Frame *frame, Function *callable, ArObject **argv, ArSize argc);
}

#endif // !ARGON_OBJECT_FRAME_H_
