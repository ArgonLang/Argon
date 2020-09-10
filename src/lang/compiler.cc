// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include "lang/parser.h"
#include "compiler.h"

using namespace argon::memory;
using namespace argon::lang;
using namespace argon::object;

Compiler::Compiler() : Compiler(nullptr) {}

Compiler::Compiler(argon::object::Map *statics_globals) {
    if ((this->statics_globals_ = statics_globals) == nullptr) {
        if ((this->statics_globals_ = MapNew()) == nullptr)
            throw std::bad_alloc();
    }
}

Compiler::~Compiler() {
    // ???
}

argon::object::Code *Compiler::Compile(std::istream *source) {
    Parser parser(source);

    auto program = parser.Parse();

    // Let's start creating a new context
    this->EnterContext(program->filename, TUScope::MODULE);

    // Cycle through program statements and call main compilation function
    for (auto &stmt:program->body)
        this->CompileCode(stmt);

    return nullptr;
}

void Compiler::EnterContext(const std::string &name, TUScope scope) {
    auto tu = AllocObject<TranslationUnit>(name, scope);
    tu->prev = this->unit_;
    this->unit_ = tu;

    // Create a new first BasicBlock
    this->unit_->BlockNew();
}

void Compiler::ExitContext() {
    auto tmp = this->unit_;
    this->unit_ = tmp->prev;
    FreeObject(tmp);
}

void Compiler::CompileCode(const ast::NodeUptr &stmt) {
#define TARGET_TYPE(type)   case ast::NodeType::type:

    switch (stmt->type) {
        TARGET_TYPE(ALIAS)
            break;
        TARGET_TYPE(ASSIGN)
            break;
        TARGET_TYPE(BINARY_OP)
            break;
        TARGET_TYPE(BLOCK)
            break;
        TARGET_TYPE(BREAK)
            break;
        TARGET_TYPE(CALL)
            break;
        TARGET_TYPE(CASE)
            break;
        TARGET_TYPE(COMMENT)
            break;
        TARGET_TYPE(CONSTANT)
            break;
        TARGET_TYPE(CONTINUE)
            break;
        TARGET_TYPE(DEFER)
            break;
        TARGET_TYPE(ELLIPSIS)
            break;
        TARGET_TYPE(ELVIS)
            break;
        TARGET_TYPE(EQUALITY)
            break;
        TARGET_TYPE(EXPRESSION)
            break;
        TARGET_TYPE(FALLTHROUGH)
            break;
        TARGET_TYPE(FOR)
            break;
        TARGET_TYPE(FOR_IN)
            break;
        TARGET_TYPE(FUNC)
            break;
        TARGET_TYPE(GOTO)
            break;
        TARGET_TYPE(IDENTIFIER)
            break;
        TARGET_TYPE(IF)
            break;
        TARGET_TYPE(IMPL)
            break;
        TARGET_TYPE(IMPORT)
            break;
        TARGET_TYPE(IMPORT_FROM)
            break;
        TARGET_TYPE(IMPORT_NAME)
            break;
        TARGET_TYPE(INDEX)
            break;
        TARGET_TYPE(LABEL)
            break;
        TARGET_TYPE(LIST)
            break;
        TARGET_TYPE(LITERAL)
            break;
        TARGET_TYPE(LOGICAL)
            break;
        TARGET_TYPE(LOOP)
            break;
        TARGET_TYPE(MAP)
            break;
        TARGET_TYPE(MEMBER)
            break;
        TARGET_TYPE(NULLABLE)
            break;
        TARGET_TYPE(PROGRAM)
            break;
        TARGET_TYPE(RELATIONAL)
            break;
        TARGET_TYPE(RETURN)
            break;
        TARGET_TYPE(SCOPE)
            break;
        TARGET_TYPE(SET)
            break;
        TARGET_TYPE(SLICE)
            break;
        TARGET_TYPE(SPAWN)
            break;
        TARGET_TYPE(STRUCT)
            break;
        TARGET_TYPE(STRUCT_INIT)
            break;
        TARGET_TYPE(SUBSCRIPT)
            break;
        TARGET_TYPE(SWITCH)
            break;
        TARGET_TYPE(TEST)
            break;
        TARGET_TYPE(TRAIT)
            break;
        TARGET_TYPE(TUPLE)
            break;
        TARGET_TYPE(UNARY_OP)
            break;
        TARGET_TYPE(UPDATE)
            break;
        TARGET_TYPE(VARIABLE)
            break;
    }

#undef TARGET_TYPE
}
