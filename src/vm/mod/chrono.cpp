// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/loop/evloop.h>

#include <vm/datatype/error.h>
#include <vm/datatype/integer.h>
#include <vm/datatype/nil.h>

#include "modules.h"

using namespace argon::vm::datatype;

ARGON_FUNCTION(chrono_sleep, sleep,
               "Suspend execution of the calling fiber for the given number of millisecond.\n"
               "\n"
               "- Parameter ms: Amount of time in milliseconds.\n"
               "- Returns: Nil.\n",
               "ui: ms", false, false) {
    auto *ui_int = (const Integer *) *args;

    UIntegerUnderlying timeout = ui_int->uint;

    if (AR_TYPEOF(*args, type_int_)) {
        timeout = ui_int->sint;

        if (ui_int->sint < 0) {
            ErrorFormat(kValueError[0], "timeout cannot be negative");
            return nullptr;
        }
    }

    if (timeout == 0)
        return (ArObject *) IncRef(Nil);

    argon::vm::loop::EventLoopSetTimeout(argon::vm::loop::GetEventLoop(), timeout);

    return nullptr;
}

const ModuleEntry chrono_entries[] = {
        MODULE_EXPORT_FUNCTION(chrono_sleep),
        ARGON_MODULE_SENTINEL
};

constexpr ModuleInit ModuleChrono = {
        "chrono",
        "This module provides various functions to manipulate time values.",
        chrono_entries,
        nullptr,
        nullptr
};
const ModuleInit *argon::vm::mod::module_chrono_ = &ModuleChrono;