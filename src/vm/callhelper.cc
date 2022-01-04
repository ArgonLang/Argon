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
