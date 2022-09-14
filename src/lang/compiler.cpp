// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/opcode.h>

#include <lang/scanner/token.h>

#include "compilererr.h"
#include "compiler.h"
#include "vm/datatype/integer.h"

using namespace argon::lang;
using namespace argon::lang::parser;
using namespace argon::vm::datatype;

int Compiler::LoadStatic(ArObject *value, bool store, bool emit) {
    ArObject *tmp;
    Integer *index;
    IntegerUnderlying idx = -1;

    IncRef(value);

    if (store) {
        // Check if value are already present in TranslationUnit
        if ((tmp = DictLookup(this->unit_->statics_map, value)) == nullptr) {
            // Value not found in the current TranslationUnit, trying in global_statics
            if ((tmp = DictLookup(this->statics_globals_, value)) != nullptr) {
                // Recover already existing object and discard the actual one
                Release(value);
                value = tmp;
            } else if (!DictInsert(this->statics_globals_, value, value)) {
                Release(value);
                throw DatatypeException();
            }

            if ((index = UIntNew(this->unit_->statics->length)) == nullptr) {
                Release(value);
                throw DatatypeException();
            }

            if (!DictInsert(this->unit_->statics_map, value, (ArObject *) index)) {
                Release(value);
                Release(index);
                throw DatatypeException();
            }

            Release(&index);
        } else {
            idx = ((Integer *) tmp)->sint;
            Release(tmp);
        }
    }

    if (!store || idx == -1) {
        idx = (IntegerUnderlying) this->unit_->statics->length;
        if (!ListAppend(this->unit_->statics, value)) {
            Release(value);
            throw DatatypeException();
        }
    }

    Release(value);

    if (emit)
        this->unit_->Emit(vm::OpCode::LSTATIC, (int) idx, nullptr, nullptr);

    return (int) idx;
}

void Compiler::Binary(const parser::Binary *binary) {
    // todo: check node type

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

void Compiler::CompileLTDS(const Unary *list) {
    ARC iter;
    ARC tmp;

    vm::OpCode code;
    int items = 0;

    iter = IteratorGet(list->value, false);
    if (!iter)
        throw DatatypeException();

    while ((tmp = IteratorNext(iter.Get()))) {
        this->Expression((Node *) tmp.Get());
        items++;
    }

    switch (list->node_type) {
        case NodeType::LIST:
            code = vm::OpCode::MKLT;
            break;
        case NodeType::TUPLE:
            code = vm::OpCode::MKTP;
            break;
        case NodeType::DICT:
            code = vm::OpCode::MKDT;
            break;
        case NodeType::SET:
            code = vm::OpCode::MKST;
            break;
        default:
            throw CompilerException("");
    }

    this->unit_->DecrementStack(items);

    this->unit_->Emit(code, items, nullptr, &list->loc);
}

void Compiler::CompileTest(const parser::Binary *test) {
    auto *cursor = test;
    BasicBlock *end;
    int deep = 0;

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    while (cursor->left->token_type == scanner::TokenType::AND || cursor->left->token_type == scanner::TokenType::OR) {
        cursor = (const parser::Binary *) cursor->left;
        deep++;
    }

    try {
        this->Expression(cursor->left);

        do {
            if (cursor->token_type == scanner::TokenType::AND)
                this->unit_->Emit(vm::OpCode::JFOP, 0, end, &cursor->loc);
            else if (cursor->token_type == scanner::TokenType::OR)
                this->unit_->Emit(vm::OpCode::JTOP, 0, end, &cursor->loc);
            else
                throw CompilerException("invalid TokenType for CompileTest");

            if (!this->unit_->BlockNew())
                throw DatatypeException();

            this->Expression(cursor->right);

            deep--;

            cursor = test;
            for (int i = 0; i < deep; i++)
                cursor = (const parser::Binary *) test->left;
        } while (deep >= 0);
    } catch (const std::exception &) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileUnary(const parser::Unary *unary) {
    this->Expression((const Node *) unary->value);

    switch (unary->token_type) {
        case scanner::TokenType::EXCLAMATION:
            this->unit_->Emit(vm::OpCode::NOT, &unary->loc);
            break;
        case scanner::TokenType::TILDE:
            this->unit_->Emit(vm::OpCode::INV, &unary->loc);
            break;
        case scanner::TokenType::PLUS:
            this->unit_->Emit(vm::OpCode::POS, &unary->loc);
            break;
        case scanner::TokenType::MINUS:
            this->unit_->Emit(vm::OpCode::NEG, &unary->loc);
            break;
        default:
            throw CompilerException("invalid TokenType for CompileUnary");
    }
}

void Compiler::Expression(const Node *node) {
    switch (node->node_type) {
        case NodeType::LITERAL:
            this->LoadStatic(((const Unary *) node)->value, true, true);
            break;
        case NodeType::BINARY:
            if (node->token_type == scanner::TokenType::AND || node->token_type == scanner::TokenType::OR) {
                this->CompileTest((const parser::Binary *) node);
                break;
            }

            this->Binary((const parser::Binary *) node);
            break;
        case NodeType::UNARY:
            this->CompileUnary((const Unary *) node);
            break;
        case NodeType::LIST:
        case NodeType::TUPLE:
        case NodeType::DICT:
        case NodeType::SET:
            this->CompileLTDS((const Unary *) node);
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