// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/error.h>
#include <object/datatype/integer.h>

#include "basicblock.h"
#include "compiler.h"

using namespace argon::lang::scanner2;
using namespace argon::lang::parser;
using namespace argon::lang::compiler;
using namespace argon::object;

bool Compiler::Compile_(Node *node) {
    if (AR_TYPEOF(node, type_ast_expression_))
        return this->CompileExpression((Unary *) ((Unary *) node)->value);

    ErrorFormat(type_compile_error_, "invalid AST node: %s", AR_TYPE_NAME(node));
    return false;
}

bool Compiler::CompileBinary(argon::lang::parser::Binary *expr) {
    bool ok = false;

    if (!this->CompileExpression((Unary *) expr->left))
        return false;

    if (!this->CompileExpression((Unary *) expr->right))
        return false;

    switch (expr->kind) {
        case TokenType::PLUS:
            ok = this->Emit(OpCodes::ADD, 0);
            break;
        case TokenType::MINUS:
            ok = this->Emit(OpCodes::SUB, 0);
            break;
        case TokenType::ASTERISK:
            ok = this->Emit(OpCodes::MUL, 0);
            break;
        case TokenType::SLASH:
            ok = this->Emit(OpCodes::DIV, 0);
            break;
        case TokenType::SLASH_SLASH:
            ok = this->Emit(OpCodes::IDIV, 0);
            break;
        case TokenType::PERCENT:
            ok = this->Emit(OpCodes::MOD, 0);
            break;
        default:
            break;
    }

    return ok;
}

bool Compiler::CompileExpression(Node *expr) {
    if (AR_TYPEOF(expr, type_ast_literal_)) {
        if (!IsHashable(((Unary *) expr)->value))
            return (Code *) ErrorFormat(type_compile_error_, "");

        if (this->PushStatic(((Unary *) expr)->value, true, true) >= 0)
            return true;
    } else if (AR_TYPEOF(expr, type_ast_binary_))
        return this->CompileBinary((Binary *) expr);

    return false;
}

int Compiler::PushStatic(ArObject *obj, bool store, bool emit) {
    ArObject *tmp = nullptr;
    ArObject *index = nullptr;
    IntegerUnderlying idx = -1;

    IncRef(obj);

    if (store) {
        // check if object is already present in TranslationUnit
        if ((tmp = MapGetNoException(this->unit_->statics_map, obj)) == nullptr) {
            // Object not found in the current TranslationUnit, trying in global_statics
            if ((tmp = MapGetNoException(this->statics_globals_, obj)) != nullptr) {
                // recover already existing object and discard the actual one
                Release(obj);
                obj = tmp;
            } else {
                if (!MapInsert(this->statics_globals_, obj, obj))
                    goto ERROR;
            }

            if ((index = IntegerNew(this->unit_->statics->len)) == nullptr)
                goto ERROR;

            // Add to local map
            if (!MapInsert(this->unit_->statics_map, obj, index))
                goto ERROR;

            Release(&index);
        } else
            idx = ((Integer *) tmp)->integer;
    }

    if (!store || idx == -1) {
        idx = this->unit_->statics->len;
        if (!ListAppend(this->unit_->statics, obj))
            goto ERROR;
    }

    Release(tmp);
    Release(obj);

    if (emit && !this->Emit(OpCodes::LSTATIC, (int) idx))
        return -1;

    return (int) idx;

    ERROR:
    Release(tmp);
    Release(index);
    Release(obj);
    return -1;
}

bool Compiler::Emit(OpCodes op, int arg) {
    Instr *instr;

    switch (op) {
        case OpCodes::LSTATIC:
            TranslationUnitIncStack(this->unit_, 1);
            break;
        case OpCodes::ADD:
        case OpCodes::SUB:
        case OpCodes::MUL:
        case OpCodes::DIV:
        case OpCodes::IDIV:
        case OpCodes::MOD:
            TranslationUnitDecStack(this->unit_, 1);
            break;
        default:
            break;
    }

    if ((instr = BasicBlockAddInstr(this->unit_->bb.cur, op, arg)) == nullptr)
        return false;

    return true;
}

bool Compiler::TScopeNew(String *name, TUScope scope) {
    auto *unit = TranslationUnitNew(name, scope);

    if (unit != nullptr) {
        // Create first BasicBlock
        if (TranslationUnitBlockNew(unit) == nullptr) {
            TranslationUnitDel(unit);
            return false;
        }

        unit->prev = this->unit_;
        this->unit_ = unit;
        return true;
    }

    return false;
}

void Compiler::TScopeClear() {
    while ((this->unit_ = TranslationUnitDel(this->unit_)) != nullptr);
}

void Compiler::TScopeExit() {
    if (this->unit_ != nullptr)
        this->unit_ = TranslationUnitDel(this->unit_);
}

Code *Compiler::Compile(File *node) {
    ArObject *decl_iter;
    ArObject *decl;

    if (!AR_TYPEOF(node, type_ast_file_))
        return (Code *) ErrorFormat(type_compile_error_, "expected %s node, found: %s", type_ast_file_->name,
                                    AR_TYPE_NAME(node));

    // Initialize global_statics
    if (this->statics_globals_ == nullptr) {
        if ((this->statics_globals_ = MapNew()) == nullptr)
            return nullptr;
    }

    // Let's start creating a new context
    if (!this->TScopeNew(node->name, TUScope::MODULE))
        return nullptr;

    if ((decl_iter = IteratorGet(node->decls)) == nullptr) {
        this->TScopeClear();
        return nullptr;
    }

    // Cycle through program statements and call main compilation function
    while ((decl = IteratorNext(decl_iter)) != nullptr) {
        if (!this->Compile_((Node *) decl)) {
            this->TScopeClear();
            Release(decl_iter);
            Release(decl);
            return nullptr;
        }

        Release(decl);
    }
    Release(decl_iter);

    return nullptr;
}
