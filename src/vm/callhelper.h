// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_CALLHELPER_H_
#define ARGON_VM_CALLHELPER_H_

#include <lang/opcodes.h>

#include <object/arobject.h>
#include <object/datatype/function.h>
#include <object/datatype/list.h>

#include "frame.h"

namespace argon::vm {
    struct CallHelper {
        Frame *frame;
        argon::object::Function *func;
        argon::object::List *list_params;
        argon::object::ArObject **params;

        argon::lang::OpCodeCallFlags flags;

        unsigned short stack_offset;
        unsigned short local_args;
        unsigned short total_args;

        [[nodiscard]] argon::object::ArObject *BindCall();

        [[nodiscard]] argon::object::ArObject *MkCurrying() const;

        bool CallSpreadExpansion();

        bool CheckVariadic();

        [[nodiscard]] bool IsPartialApplication() const;

        bool PrepareCall(Frame *wframe);

        bool SpawnFunction(ArRoutine *routine);

        void ClearCall() const;
    };

    bool CallHelperInit(CallHelper *helper, Frame *frame);

    bool CallHelperInit(CallHelper *helper, argon::object::ArObject **argv, int argc);

    bool CallHelperCall(CallHelper *helper, Frame *frame, argon::object::ArObject **result);

} // namespace argon::vm

#endif // !ARGON_VM_CALLHELPER_H_
