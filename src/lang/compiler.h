// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_H_
#define ARGON_LANG_COMPILER_H_

#include <vm/datatype/code.h>

#include <lang/parser/node.h>

namespace argon::lang {
    class Compiler{
    public:
        Compiler() noexcept = default;

        ~Compiler();

        [[nodiscard]] argon::vm::datatype::Code *Compile(argon::lang::parser::File *node);
    };
}

#endif // !ARGON_LANG_COMPILER_H_
