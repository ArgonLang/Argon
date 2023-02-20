// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_MOD_MODULES_H_
#define ARGON_VM_MOD_MODULES_H_

#include <vm/datatype/module.h>

namespace argon::vm::mod {
    extern const datatype::ModuleInit *module_builtins_;
    extern const datatype::ModuleInit *module_gc_;
    extern const datatype::ModuleInit *module_io_;

} // namespace argon::vm::mod

#endif // !ARGON_VM_MOD_MODULES_H_
