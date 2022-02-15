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

argon::object::ArObject *CallHelper::BindCall() {
    ArObject *ret;

    if (this->IsPartialApplication()) {
        ErrorFormat(type_type_error_, "%s() takes %d argument, but %d were given",
                    this->func->name->buffer, this->func->arity, this->total_args);
        this->ClearCall();
        return nullptr;
    }

    if (!this->CheckVariadic())
        return nullptr;

    if (this->list_params != nullptr) {
        ret = FunctionNew(this->func, this->list_params);
        this->ClearCall();
    } else
        ret = this->MkCurrying();

    return ret;
}

argon::object::ArObject *CallHelper::MkCurrying() const {
    List *currying;
    ArObject *ret;

    if (this->func->arity > 0 && this->total_args == 0) {
        ErrorFormat(type_type_error_, "%s() takes %d argument, but 0 were given",
                    this->func->name->buffer, this->func->arity);
        this->ClearCall();
        return nullptr;
    }

    if ((currying = ListNew(this->local_args)) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < this->local_args; i++) {
        if (!ListAppend(currying, this->params[i])) {
            Release(currying);
            return nullptr;
        }
    }

    ret = FunctionNew(this->func, currying);
    Release(currying);
    this->ClearCall();

    return ret;
}

bool CallHelper::CallSpreadExpansion() {
    ArObject **stack = this->frame->eval_stack - this->local_args;

    if ((this->list_params = ListNew(this->local_args)) != nullptr) {
        for (int i = 0; i < this->local_args - 1; i++)
            ListAppend(this->list_params, stack[i]);

        if (!ListConcat(this->list_params, stack[this->local_args - 1])) {
            Release(this->list_params);
            return false;
        }

        // Update locals_args
        this->local_args = this->list_params->len;
        this->params = this->list_params->objects;

        return true;
    }

    return false;
}

bool CallHelper::CheckVariadic() {
    unsigned short exceeded = this->total_args - this->func->arity;
    ArObject *ret;

    if (this->total_args > this->func->arity) {
        if (!this->func->IsVariadic()) {
            ErrorFormat(type_type_error_, "%s() takes %d argument, but %d were given",
                        this->func->name->buffer, this->func->arity, this->total_args);
            this->ClearCall();
            return false;
        }

        if (!this->func->IsNative()) {
            ret = RestElementToList(this->params + (this->local_args - exceeded), exceeded);

            if (ret == nullptr) {
                this->ClearCall();
                return false;
            }

            Release(this->params[(this->local_args - exceeded)]);
            this->params[(this->local_args - exceeded)] = ret;
            this->local_args -= exceeded - 1;
        }
    }

    return true;
}

inline bool CallHelper::IsPartialApplication() const {
    return this->total_args < this->func->arity;
}

bool CallHelper::PrepareCall(Frame *wframe) {
    this->stack_offset = argon::lang::I32Arg(wframe->instr_ptr);
    this->flags = (argon::lang::OpCodeCallFlags) argon::lang::I32ExtractFlag(wframe->instr_ptr);
    this->frame = wframe;

    this->func = (Function *) *(wframe->eval_stack - this->stack_offset - 1);

    this->local_args = this->stack_offset;

    if (AR_TYPEOF(this->func, type_function_)) {
        if (this->local_args > 0 && *(wframe->eval_stack - this->local_args) == nullptr)
            this->local_args--;

        this->params = wframe->eval_stack - this->local_args;

        if (ENUMBITMASK_ISTRUE(this->flags, argon::lang::OpCodeCallFlags::SPREAD))
            if (!this->CallSpreadExpansion())
                return false;

        this->total_args = this->local_args;

        if (this->func->currying != nullptr)
            this->total_args += this->func->currying->len;

        return true;
    }

    ErrorFormat(type_type_error_, "'%s' object is not callable", AR_TYPE_NAME(this->func));
    return false;
}

bool CallHelper::SpawnFunction(ArRoutine *routine) {
    ArRoutine *s_routine;
    Frame *s_frame;

    Function *fn;
    Code *code;

    if ((fn = (Function *) this->BindCall()) == nullptr)
        return false;

    code = fn->code;

    if (fn->IsNative()) {
        if ((code = CodeNewNativeWrapper(fn)) == nullptr) {
            Release(fn);
            return false;
        }

        fn = nullptr;
    }

    if ((s_frame = FrameNew(code, this->frame->globals, this->frame->proxy_globals)) == nullptr) {
        Release(fn);
        return false;
    }

    if (fn != nullptr) {
        FrameFillForCall(s_frame, fn, nullptr, 0);
        Release(fn);
    }

    if ((s_routine = RoutineNew(s_frame, routine)) == nullptr) {
        FrameDel(s_frame);
        return false;
    }

    if (!Spawn(s_routine)) {
        RoutineDel(s_routine);
        return false;
    }

    return true;
}

void CallHelper::ClearCall() const {
    Release(this->list_params);

    // If routine status == ArRoutineStatus::BLOCKED, do not attempt to clear the stack!
    // Because will be used at the routine resume.
    if (argon::vm::GetRoutine()->status == ArRoutineStatus::BLOCKED)
        return;

    for (ArSize i = this->stack_offset + 1; i > 0; i--)
        Release(*(--this->frame->eval_stack));
}

// NEW IMPL

ArObject *MakePartialApplication(CallHelper *helper) {
    List *currying;
    ArObject *ret;

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

    FrameFillForCall(fn_frame, helper->func, helper->params, helper->local_args);
    CallHelperClear(helper, frame);

    // Invoke
    frame->instr_ptr += sizeof(argon::lang::Instr32);
    fn_frame->back = frame;
    GetRoutine()->frame = fn_frame;
    return true;
}