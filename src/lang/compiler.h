// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_H_
#define ARGON_LANG_COMPILER_H_

#include <istream>

#include "ast/ast.h"

namespace lang {
    class compiler {
        void CompileLiteral(const ast::Literal *literal);

        void CompileCode(const ast::NodeUptr &node);

    public:
        void Compile(std::istream *source);
    };

} // namespace lang

#endif // !ARGON_LANG_COMPILER_H_
