// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/opcode.h>

#include <lang/scanner/token.h>

#include "compilererr.h"
#include "compiler.h"

using namespace argon::lang;
using namespace argon::lang::parser;
using namespace argon::vm::datatype;

void Compiler::Binary(const parser::Binary *binary) {
    this->Expression(binary->left);
    this->Expression(binary->right);

    switch (binary->token_type) {
        case lang::scanner::TokenType::PLUS:
            this->unit_->Emit(vm::OpCode::ADD, &binary->loc);
            break;
        case lang::scanner::TokenType::MINUS:
            this->unit_->Emit(vm::OpCode::SUB, &binary->loc);
            break;
        case lang::scanner::TokenType::ASTERISK:
            this->unit_->Emit(vm::OpCode::MUL, &binary->loc);
            break;
        case lang::scanner::TokenType::SLASH:
            this->unit_->Emit(vm::OpCode::DIV, &binary->loc);
            break;
        case lang::scanner::TokenType::SLASH_SLASH:
            this->unit_->Emit(vm::OpCode::IDIV, &binary->loc);
            break;
        case lang::scanner::TokenType::PERCENT:
            this->unit_->Emit(vm::OpCode::MOD, &binary->loc);
            break;

            // SHIFT
        case lang::scanner::TokenType::SHL:
            this->unit_->Emit(vm::OpCode::SHL, &binary->loc);
            break;
        case lang::scanner::TokenType::SHR:
            this->unit_->Emit(vm::OpCode::SHR, &binary->loc);
            break;

            // EQUALITY
        case lang::scanner::TokenType::EQUAL_EQUAL:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::EQ, nullptr, &binary->loc);
            break;
        case lang::scanner::TokenType::EQUAL_STRICT:
            this->unit_->Emit(vm::OpCode::EQST, (int) CompareMode::EQ, nullptr, &binary->loc);
            break;
        case lang::scanner::TokenType::NOT_EQUAL:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::NE, nullptr, &binary->loc);
            break;
        case lang::scanner::TokenType::NOT_EQUAL_STRICT:
            this->unit_->Emit(vm::OpCode::EQST, (int) CompareMode::NE, nullptr, &binary->loc);
            break;

            // LOGICAL
        case lang::scanner::TokenType::AMPERSAND:
            this->unit_->Emit(vm::OpCode::LAND, &binary->loc);
            break;
        case lang::scanner::TokenType::PIPE:
            this->unit_->Emit(vm::OpCode::LOR, &binary->loc);
            break;
        case lang::scanner::TokenType::CARET:
            this->unit_->Emit(vm::OpCode::LXOR, &binary->loc);
            break;

            // RELATIONAL
        case lang::scanner::TokenType::GREATER:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::GR, nullptr, &binary->loc);
            break;
        case lang::scanner::TokenType::GREATER_EQ:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::GRQ, nullptr, &binary->loc);
            break;
        case lang::scanner::TokenType::LESS:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::LE, nullptr, &binary->loc);
            break;
        case lang::scanner::TokenType::LESS_EQ:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::LEQ, nullptr, &binary->loc);
            break;
        default:
            // TODO: fix
            throw CompilerException("");
    }
}

void Compiler::Compile(const Node *node) {
    switch (node->node_type) {
        case parser::NodeType::EXPRESSION:
            this->Expression((Node *) ((const Unary *) node)->value);
            this->unit_->Emit(vm::OpCode::POP, nullptr);
            break;
        default:
            assert(false);
    }
}

void Compiler::Expression(const Node *node) {
    switch (node->node_type) {
        case NodeType::BINARY:
            this->Binary((const parser::Binary *) node);
            break;
    }
}

void Compiler::TUScopeEnter(String *name, SymbolType context) {
    TranslationUnit *unit;
    SymbolT *symt;

    if (this->unit_ == nullptr) {
        assert(context == SymbolType::MODULE);

        if ((symt = SymbolNew(name)) == nullptr)
            throw DatatypeException();
    } else {
        if ((symt = SymbolInsert(this->unit_->symt, name, nullptr, context)) == nullptr)
            throw DatatypeException();
    }

    unit = TranslationUnitNew(this->unit_, name, symt);

    Release(symt);

    if (unit == nullptr)
        throw DatatypeException();

    this->unit_ = unit;
}

void Compiler::TUScopeExit() {
    if (this->unit_ != nullptr) {
        assert(this->unit_->stack.current == 0);
        this->unit_ = TranslationUnitDel(this->unit_);
    }
}

Compiler::~Compiler() {

}

Code *Compiler::Compile(File *node) {
    // Initialize global_statics
    if (this->statics_globals_ == nullptr) {
        this->statics_globals_ = DictNew();
        if (this->statics_globals_ == nullptr)
            return nullptr;
    }

    try {
        ARC decl_iter;
        ARC decl;

        // Let's start creating a new context
        this->TUScopeEnter(nullptr, SymbolType::MODULE);

        decl_iter = IteratorGet((ArObject *) node->statements, false);
        if (!decl_iter)
            throw DatatypeException();

        // Cycle through program statements and call main compilation function
        while ((decl = IteratorNext(decl_iter.Get())))
            this->Compile((Node *) decl.Get());

    } catch (std::exception) {

    }

    return nullptr;
}