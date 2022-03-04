// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_CALLHELPER_H_
#define ARGON_VM_CALLHELPER_H_

#include <lang/opcodes.h>

#include <object/arobject.h>
#include <object/datatype/frame.h>
#include <object/datatype/function.h>
#include <object/datatype/list.h>

namespace argon::vm {
    struct CallHelper {
        argon::object::Function *func;
        argon::object::List *list_params;
        argon::object::ArObject **params;

        argon::lang::OpCodeCallFlags flags;

        unsigned short stack_offset;
        unsigned short local_args;
        unsigned short total_args;
    };

    bool CallHelperInit(CallHelper *helper, argon::object::Frame *frame);

    bool CallHelperInit(CallHelper *helper, argon::object::Function *callable, argon::object::ArObject **argv, unsigned short argc);

    bool CallHelperCall(CallHelper *helper, argon::object::Frame **in_out_frame, argon::object::ArObject **result);

    bool CallHelperSpawn(CallHelper *helper, argon::object::Frame *frame);

    [[nodiscard]] argon::object::Function *CallHelperBind(CallHelper *helper, argon::object::Frame *frame);

} // namespace argon::vm

#endif // !ARGON_VM_CALLHELPER_H_
