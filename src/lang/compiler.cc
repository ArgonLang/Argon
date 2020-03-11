// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory>

#include "compiler.h"
#include "parser.h"

using namespace lang;
using namespace lang::ast;

void compiler::Compile(std::istream *source) {
    Parser parser(source);
    std::unique_ptr<ast::Program> program = parser.Parse();

    for (auto &stmt : program->body)
        this->CompileCode(stmt);
}

void compiler::CompileCode(const ast::NodeUptr &node) {
    switch (node->type) {
        case NodeType::IDENTIFIER:
            break;
        case NodeType::LITERAL:
            this->CompileLiteral(CastNode<Literal>(node));
            break;
    }
}

void compiler::CompileLiteral(const ast::Literal *literal) {

}
