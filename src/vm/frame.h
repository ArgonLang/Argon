// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_FRAME_H_
#define ARGON_VM_FRAME_H_

#include <object/code.h>

namespace argon::vm {
    struct Frame {
        Frame *back;

        object::Code *code;

        unsigned char *instr_ptr;

        object::Object **eval_stack;

        size_t eval_index;

        unsigned char *extras_[];
    };

    Frame *FrameNew(object::Code *code);

    void FrameDel(Frame *frame);

} // namespace argon::vm

#endif // !ARGON_VM_FRAME_H_
