// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "modules.h"

using namespace argon::vm::datatype;

const ModuleEntry builtins_entries[] = {
        ARGON_MODULE_SENTINEL
};

constexpr ModuleInit ModuleBuiltins = {
        "builtins",
        "Built-in functions and other things.",
        builtins_entries,
        nullptr,
        nullptr
};
const ModuleInit *argon::vm::mod::module_builtins_ = &ModuleBuiltins;