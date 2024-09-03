// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/runtime.h>

#include <argon/vm/datatype/nil.h>
#include <argon/vm/datatype/traceback.h>

#include <argon/vm/mod/modules.h>

using namespace argon::vm::datatype;

Traceback *extract_stack(argon::vm::Fiber *fiber, argon::vm::Frame *frame) {
    Traceback *tb_base = nullptr;
    Traceback *tb_current = nullptr;
    auto *l_panic = fiber->panic;

    while (frame != nullptr) {
        auto *code = frame->code;

        auto pc_offset = frame->instr_ptr - code->instr;
        auto lineno = code->GetLineMapping(pc_offset);

        Traceback *tb;
        if (l_panic != nullptr && l_panic->frame == frame)
            tb = TracebackNew(frame->code, l_panic, lineno, pc_offset);
        else
            tb = TracebackNew(frame->code, nullptr, lineno, pc_offset);

        if (tb == nullptr) {
            Release(tb_base);

            return nullptr;
        }

        if (tb_base == nullptr) {
            tb_base = tb;
            tb_current = tb;
        } else {
            tb_current->back = tb;
            tb_current = tb;
        }

        frame = frame->back;
    }

    return tb_base;
}

ARGON_FUNCTION(traceback_extract_panic, extract_panic,
               "Extract the traceback of the current panic.\n"
               "\n"
               "This function retrieves the traceback information from the current panic state.\n"
               "If there is no active panic, it returns Nil.\n"
               "\n"
               "- Returns: A Traceback object representing the current panic's traceback, or Nil if there's no active panic.\n",
               nullptr, false, false) {
    auto *fiber = argon::vm::GetFiber();

    if (fiber->panic == nullptr)
        return ARGON_NIL_VALUE;

    return (ArObject *) extract_stack(fiber, fiber->panic->frame);
}

ARGON_FUNCTION(traceback_extract_stack, extract_stack,
               "Extract the current stack trace.\n"
               "\n"
               "This function captures the current execution stack and creates a Traceback object from it.\n"
               "\n"
               "- Returns: A Traceback object representing the current execution stack.\n",
               nullptr, false, false) {
    return (ArObject *) extract_stack(argon::vm::GetFiber(), argon::vm::GetFrame());
}

ARGON_FUNCTION(traceback_extract_panicinfo, extract_panicinfo,
               "Extract detailed information about the current panic.\n"
               "\n"
               "This function retrieves detailed information about the current panic, including the code object, "
               "line number, and instruction pointer offset where the panic occurred.\n"
               "If there is no active panic, it returns Nil.\n"
               "\n"
               "- Returns: A Traceback object with detailed panic information, or Nil if there's no active panic.\n",
               nullptr, false, false) {
    auto *fiber = argon::vm::GetFiber();
    auto *panic = fiber->panic;

    if (fiber->panic == nullptr)
        return ARGON_NIL_VALUE;

    auto *frame = panic->frame;
    auto *code = panic->frame->code;

    return (ArObject *) TracebackNew(code,
                                     panic,
                                     code->GetLineMapping(frame->instr_ptr - code->instr),
                                     frame->instr_ptr - code->instr);
}

const ModuleEntry traceback_entries[] = {
        MODULE_EXPORT_TYPE(type_traceback_),

        MODULE_EXPORT_FUNCTION(traceback_extract_panic),
        MODULE_EXPORT_FUNCTION(traceback_extract_panicinfo),
        MODULE_EXPORT_FUNCTION(traceback_extract_stack),

        ARGON_MODULE_SENTINEL
};

constexpr ModuleInit ModuleTraceback = {
        "argon:traceback",
        "This module offers tools to extract stack traces, panic information, and create Traceback objects, which "
        "are essential for debugging and error reporting in Argon.",
        nullptr,
        traceback_entries,
        nullptr,
        nullptr
};
const ModuleInit *argon::vm::mod::module_traceback_ = &ModuleTraceback;
