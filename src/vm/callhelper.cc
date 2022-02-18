// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/error.h>

#include "arroutine.h"
#include "runtime.h"
#include "callhelper.h"

using namespace argon::object;
using namespace argon::vm;

ArObject *RestElementToList(ArObject **args, ArSize count) {
    List *rest = ListNew(count);

    if (rest != nullptr) {
        for (ArSize i = 0; i < count; i++) {
            if (!ListAppend(rest, args[i])) {
                Release(rest);
                return nullptr;
            }
        }
    }

    return rest;
}

bool SpreadExpansion(CallHelper *helper) {
    ArObject *spread_obj = helper->params[helper->local_args - 1];

    // TODO: check spread_obj

    if ((helper->list_params = ListNew(helper->local_args)) == nullptr)
        return false;

    for (int i = 0; i < helper->local_args - 1; i++)
        ListAppend(helper->list_params, helper->params[i]);

    if (!ListConcat(helper->list_params, spread_obj)) {
        Release((ArObject **) &helper->list_params);
        return false;
    }

    helper->local_args = helper->list_params->len;
    helper->params = helper->list_params->objects;

    return true;
}

bool CheckArity(CallHelper *helper) {
    unsigned short exceeded = helper->total_args - helper->func->arity;
    ArObject *ret;

    if (helper->total_args > helper->func->arity) {
        if (!helper->func->IsVariadic()) {
            ErrorFormat(type_type_error_, "%s() takes %d argument, but %d were given",
                        helper->func->name->buffer, helper->func->arity, helper->total_args);
            return false;
        }

        if (!helper->func->IsNative()) {
            if ((ret = RestElementToList(helper->params + (helper->local_args - exceeded), exceeded)) == nullptr)
                return false;

            // TODO: CHECK NATIVE CALL
            Release(helper->params[(helper->local_args - exceeded)]);
            helper->params[(helper->local_args - exceeded)] = ret;
            helper->local_args -= exceeded - 1;
        }
    }

    return true;
}

void CallHelperClear(CallHelper *helper, Frame *frame) {
    Release(helper->list_params);

    if (frame != nullptr) {
        // If routine status == ArRoutineStatus::BLOCKED, do not attempt to clear the stack!
        // Because will be used at the routine resume.
        if (argon::vm::GetRoutine()->status == ArRoutineStatus::BLOCKED)
            return;

        for (ArSize i = helper->stack_offset + 1; i > 0; i--)
            Release(*(--frame->eval_stack));
    }
}

Function *MakePartialApplication(CallHelper *helper) {
    List *currying;
    Function *ret;

    if (helper->func->arity > 0 && helper->total_args == 0) {
        ErrorFormat(type_type_error_, "%s() takes %d argument, but 0 were given",
                    helper->func->name->buffer, helper->func->arity);
        return nullptr;
    }

    if ((currying = ListNew(helper->local_args)) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < helper->local_args; i++)
        ListAppend(currying, helper->params[i]);

    ret = FunctionNew(helper->func, currying);
    Release(currying);

    return ret;
}

bool argon::vm::CallHelperInit(CallHelper *helper, Frame *frame) {
    // Extract call flags
    helper->flags = (argon::lang::OpCodeCallFlags) argon::lang::I32ExtractFlag(frame->instr_ptr);

    // Extract number of argument passed in this call
    helper->local_args = argon::lang::I32Arg(frame->instr_ptr);

    // Extract function object
    helper->func = (Function *) *(frame->eval_stack - helper->local_args - 1);

    // TODO check
    helper->stack_offset = helper->local_args;

    if (!AR_TYPEOF(helper->func, type_function_)) {
        ErrorFormat(type_type_error_, "'%s' object is not callable", AR_TYPE_NAME(helper->func));
        return false;
    }

    // Ignore the nullptr instance value for FUNCTION(not method!) loaded through LDMETH
    if (helper->local_args > 0 && *(frame->eval_stack - helper->local_args) == nullptr)
        helper->local_args--;

    helper->params = frame->eval_stack - helper->local_args;

    helper->list_params = nullptr;
    if (ENUMBITMASK_ISTRUE(helper->flags, argon::lang::OpCodeCallFlags::SPREAD)) {
        if (!SpreadExpansion(helper))
            return false;
    }

    helper->total_args = helper->local_args;

    if (helper->func->currying != nullptr)
        helper->total_args += helper->func->currying->len;

    return true;
}

bool argon::vm::CallHelperInit(CallHelper *helper, ArObject **argv, int argc) {
    argon::memory::MemoryZero(helper, sizeof(CallHelper));

    helper->params = argv;
    helper->local_args = argc;
    helper->total_args = argc;

    return true;
}

bool argon::vm::CallHelperCall(CallHelper *helper, Frame *frame, ArObject **result) {
    Frame *fn_frame;

    *result = nullptr;

    // Partial application
    if (helper->total_args < helper->func->arity) {
        if ((*result = MakePartialApplication(helper)) == nullptr) {
            CallHelperClear(helper, frame);
            return false;
        }
        return true;
    }

    if (!CheckArity(helper)) {
        CallHelperClear(helper, frame);
        return false;
    }

    if (helper->func->IsNative()) {
        STWCheckpoint();

        *result = FunctionCallNative(helper->func, helper->params, helper->local_args);

        CallHelperClear(helper, frame);
        return *result != nullptr;
    }

    // Call ArgonCode
    if ((fn_frame = FrameNew(helper->func->code, helper->func->gns, nullptr)) == nullptr) {
        CallHelperClear(helper, frame);
        return false;
    }

    FrameFill(fn_frame, helper->func, helper->params, helper->local_args);
    CallHelperClear(helper, frame);

    // Invoke
    frame->instr_ptr += sizeof(argon::lang::Instr32);
    fn_frame->back = frame;
    GetRoutine()->frame = fn_frame;
    return true;
}

bool argon::vm::CallHelperSpawn(CallHelper *helper, Frame *frame) {
    ArRoutine *s_routine;
    Code *s_code;
    Function *s_func;
    Frame *s_frame;

    bool ok = false;

    if ((s_func = CallHelperBind(helper, frame)) == nullptr)
        return false;

    s_code = s_func->code;

    if (s_func->IsNative()) {
        if ((s_code = CodeNewNativeWrapper(s_func)) == nullptr) {
            Release(s_func);
            return false;
        }

        Release((ArObject **) &s_func);
    }

    if ((s_frame = FrameNew(s_code, frame->globals, frame->proxy_globals)) == nullptr)
        goto error;

    if (s_func != nullptr)
        FrameFill(s_frame, s_func, nullptr, 0);

    if ((s_routine = RoutineNew(s_frame, GetRoutine())) == nullptr) {
        Release(s_frame);
        goto error;
    }

    if (!Spawn(s_routine)) {
        // Do not attempt to call FrameDel here,
        // it will be invoked automatically by RoutineDel!
        RoutineDel(s_routine);
        goto error;
    }

    ok = true;

    error:
    if (s_func == nullptr)
        Release(s_code);

    Release(s_func);
    return ok;
}

Function *argon::vm::CallHelperBind(CallHelper *helper, Frame *frame) {
    Function *ret = nullptr;

    if (helper->total_args < helper->func->arity) {
        ErrorFormat(type_type_error_, "%s() takes %d argument, but %d were given",
                    helper->func->name->buffer, helper->func->arity, helper->total_args);
        goto ERROR;
    }

    if (!CheckArity(helper))
        goto ERROR;

    ret = helper->list_params != nullptr ?
          FunctionNew(helper->func, helper->list_params) : MakePartialApplication(helper);

    ERROR:
    CallHelperClear(helper, frame);
    return ret;
}