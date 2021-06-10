// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MODULE_MODULES_H_
#define ARGON_MODULE_MODULES_H_

#include <object/datatype/module.h>

namespace argon::module {
    extern const argon::object::ModuleInit *module_builtins_;
    extern const argon::object::ModuleInit *module_error_;
    extern const argon::object::ModuleInit *module_gc_;
    extern const argon::object::ModuleInit *module_io_;
    extern const argon::object::ModuleInit *module_math_;
    extern const argon::object::ModuleInit *module_os_;
    extern const argon::object::ModuleInit *module_random_;
    extern const argon::object::ModuleInit *module_runtime_;
} // namespace argon::module

#endif // !ARGON_MODULE_MODULES_H_
