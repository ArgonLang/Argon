// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/error.h>
#include <object/datatype/integer.h>
#include <object/datatype/nil.h>

#include "basicblock.h"
#include "compiler.h"

using namespace argon::lang::scanner2;
using namespace argon::lang::parser;
using namespace argon::lang::compiler;
using namespace argon::object;

bool Compiler::Compile_(Node *node) {
    if (AR_TYPEOF(node, type_ast_expression_)) {
        if (!this->CompileExpression((Unary *) ((Unary *) node)->value))
            return false;

        return this->Emit(OpCodes::POP, 0, nullptr);
    } else if (AR_TYPEOF(node, type_ast_let_)) {
        auto *variable = (Assignment *) node;
        PropertyType flags = PropertyType::CONST;

        if (variable->pub)
            flags |= PropertyType::PUBLIC;

        if (!this->CompileExpression((Unary *) variable->value))
            return false;

        if (!this->IdentifierNew((String *) variable->name, SymbolType::CONSTANT, flags, true))
            return false;

        return true;
    } else if (AR_TYPEOF(node, type_ast_var_)) {
        auto *variable = (Assignment *) node;
        PropertyType flags{};

        if (variable->pub)
            flags |= PropertyType::PUBLIC;
        if (variable->weak)
            flags |= PropertyType::WEAK;

        if (variable->value == nullptr) {
            if (!this->PushStatic(NilVal, true, true))
                return false;
        } else {
            if (!this->CompileExpression((Unary *) variable->value))
                return false;
        }

        if (!this->IdentifierNew((String *) variable->name, SymbolType::VARIABLE, flags, true))
            return false;

        return true;
    }

    ErrorFormat(type_compile_error_, "invalid AST node: %s", AR_TYPE_NAME(node));
    return false;
}

bool Compiler::CompileBinary(Binary *expr) {
    bool ok = false;

    if (!this->CompileExpression((Unary *) expr->left))
        return false;

    if (!this->CompileExpression((Unary *) expr->right))
        return false;

    switch (expr->kind) {
        case TokenType::PLUS:
            ok = this->Emit(OpCodes::ADD, 0, nullptr);
            break;
        case TokenType::MINUS:
            ok = this->Emit(OpCodes::SUB, 0, nullptr);
            break;
        case TokenType::ASTERISK:
            ok = this->Emit(OpCodes::MUL, 0, nullptr);
            break;
        case TokenType::SLASH:
            ok = this->Emit(OpCodes::DIV, 0, nullptr);
            break;
        case TokenType::SLASH_SLASH:
            ok = this->Emit(OpCodes::IDIV, 0, nullptr);
            break;
        case TokenType::PERCENT:
            ok = this->Emit(OpCodes::MOD, 0, nullptr);
            break;

            // EQUALITY
        case TokenType::EQUAL_EQUAL:
            ok = this->Emit(OpCodes::CMP, (int) CompareMode::EQ, nullptr);
            break;
        case TokenType::NOT_EQUAL:
            ok = this->Emit(OpCodes::CMP, (int) CompareMode::NE, nullptr);
            break;

            // LOGICAL
        case TokenType::AMPERSAND:
            ok = this->Emit(OpCodes::LAND, 0, nullptr);
            break;
        case TokenType::CARET:
            ok = this->Emit(OpCodes::LXOR, 0, nullptr);
            break;
        case TokenType::PIPE:
            ok = this->Emit(OpCodes::LOR, 0, nullptr);
            break;

            // RELATIONAL
        case TokenType::GREATER:
            ok = this->Emit(OpCodes::CMP, (int) CompareMode::GE, nullptr);
            break;
        case TokenType::GREATER_EQ:
            ok = this->Emit(OpCodes::CMP, (int) CompareMode::GEQ, nullptr);
            break;
        case TokenType::LESS:
            ok = this->Emit(OpCodes::CMP, (int) CompareMode::LE, nullptr);
            break;
        case TokenType::LESS_EQ:
            ok = this->Emit(OpCodes::CMP, (int) CompareMode::LEQ, nullptr);
            break;
        default:
            break;
    }

    return ok;
}

bool Compiler::CompileUnary(Unary *expr) {
    bool ok = false;

    if (!this->CompileExpression((Node *) expr->value))
        return false;

    switch (expr->kind) {
        case TokenType::EXCLAMATION:
            ok = this->Emit(OpCodes::NOT, 0, nullptr);
            break;
        case TokenType::TILDE:
            ok = this->Emit(OpCodes::INV, 0, nullptr);
            break;
        case TokenType::PLUS:
            ok = this->Emit(OpCodes::POS, 0, nullptr);
            break;
        case TokenType::MINUS:
            ok = this->Emit(OpCodes::NEG, 0, nullptr);
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
    } else if (AR_TYPEOF(expr, type_ast_identifier_))
        return this->IdentifierLoad((String *) ((Unary *) expr)->value);
    else if (AR_TYPEOF(expr, type_ast_binary_))
        return this->CompileBinary((Binary *) expr);
    else if (AR_TYPEOF(expr, type_ast_unary_))
        return this->CompileUnary((Unary *) expr);

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

    if (emit && !this->Emit(OpCodes::LSTATIC, (int) idx, nullptr))
        return -1;

    return (int) idx;

    ERROR:
    Release(tmp);
    Release(index);
    Release(obj);
    return -1;
}

bool Compiler::Emit(OpCodes op, int arg, BasicBlock *dest) {
    Instr *instr;

    switch (op) {
        case OpCodes::LSTATIC:
        case OpCodes::LDGBL:
        case OpCodes::LDLC:
        case OpCodes::LDENC:
            TranslationUnitIncStack(this->unit_, 1);
            break;
        case OpCodes::ADD:
        case OpCodes::SUB:
        case OpCodes::MUL:
        case OpCodes::DIV:
        case OpCodes::IDIV:
        case OpCodes::MOD:
        case OpCodes::CMP:
        case OpCodes::POP:
        case OpCodes::JF:
        case OpCodes::NGV:
        case OpCodes::STLC:
            TranslationUnitDecStack(this->unit_, 1);
            break;
        default:
            break;
    }

    if ((instr = BasicBlockAddInstr(this->unit_->bb.cur, op, arg)) == nullptr)
        return false;

    instr->jmp = dest;

    return true;
}

bool Compiler::Emit(OpCodes op, unsigned char flags, unsigned short arg) {
    int combined = flags << 16 | arg;
    return this->Emit(op, combined, nullptr);
}

bool Compiler::IdentifierLoad(String *name) {
    // N.B. Unknown variable, by default does not raise an error,
    // but tries to load it from the global namespace.
    Symbol *sym;

    if ((sym = this->IdentifierLookupOrCreate(name, SymbolType::VARIABLE)) == nullptr)
        return false;

    if (this->unit_->scope == TUScope::FUNCTION || sym->nested > 0) {
        if (sym->declared) {
            if (!this->Emit(OpCodes::LDLC, (int) sym->id, nullptr))
                goto ERROR;

            Release(sym);
            return true;
        }

        if (sym->free) {
            if (!this->Emit(OpCodes::LDENC, (int) sym->id, nullptr))
                goto ERROR;

            Release(sym);
            return true;
        }
    }

    if (!this->Emit(OpCodes::LDGBL, (int) sym->id, nullptr))
        goto ERROR;

    Release(sym);
    return true;

    ERROR:
    Release(sym);
    return false;
}

bool Compiler::IdentifierNew(String *name, SymbolType stype, PropertyType ptype, bool emit) {
    List *dest = this->unit_->names;
    ArObject *arname;
    Symbol *sym;
    bool inserted;

    if ((sym = SymbolTableInsert(this->unit_->symt, name, stype, &inserted)) == nullptr)
        return false;

    sym->declared = true;

    if (this->unit_->scope != TUScope::FUNCTION || sym->nested == 0) {
        if (emit) {
            if (!this->Emit(OpCodes::NGV, (unsigned char) ptype, !inserted ? sym->id : dest->len)) {
                Release(sym);
                return false;
            }
        }

        if (!inserted) {
            Release(sym);
            return true;
        }
    } else {
        dest = this->unit_->locals;
        if (emit) {
            if (!this->Emit(OpCodes::STLC, (int) dest->len, nullptr)) {
                Release(sym);
                return false;
            }
        }
    }

    if (!inserted)
        arname = ListGetItem(!sym->free ? this->unit_->names : this->unit_->enclosed, sym->id);
    else
        arname = IncRef(name);

    sym->id = dest->len;
    Release(sym);

    if (!ListAppend(dest, arname)) {
        Release(arname);
        return false;
    }

    Release(arname);
    return true;
}

bool Compiler::TScopeNew(String *name, TUScope scope) {
    SymbolTable *table = this->symt;
    Symbol *symbol;
    TranslationUnit *unit;
    SymbolType sym_kind;

    if (this->unit_ != nullptr) {
        switch (scope) {
            case TUScope::FUNCTION:
                sym_kind = SymbolType::FUNC;
                break;
            case TUScope::STRUCT:
                sym_kind = SymbolType::STRUCT;
                break;
            case TUScope::TRAIT:
                sym_kind = SymbolType::TRAIT;
                break;
            default:
                assert(false);
        }

        if ((symbol = SymbolTableInsertNs(this->unit_->symt, name, sym_kind)) == nullptr)
            return false;

        table = symbol->symt;
        Release(symbol);
    }

    if ((unit = TranslationUnitNew(this->unit_, name, scope, table)) != nullptr) {
        this->unit_ = unit;
        return true;
    }

    return false;
}

Symbol *Compiler::IdentifierLookupOrCreate(String *name, SymbolType type) {
    List *dst = this->unit_->names;
    Symbol *sym;

    if ((sym = SymbolTableLookup(this->unit_->symt, name)) == nullptr) {
        if ((sym = SymbolTableInsert(this->unit_->symt, name, type, nullptr)) == nullptr)
            return nullptr;

        if (TranslationUnitIsFreeVar(this->unit_, name)) {
            dst = this->unit_->enclosed;
            sym->free = true;
        }

        sym->id = dst->len;

        if (!ListAppend(dst, name)) {
            Release(sym);
            return nullptr;
        }
    }

    return sym;
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

    // Initialize SymbolTable
    if (this->symt == nullptr) {
        if ((this->symt = SymbolTableNew(nullptr)) == nullptr)
            return nullptr;
    }

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
