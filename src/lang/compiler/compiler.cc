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
    } else if (AR_TYPEOF(node, type_ast_label_)) {
        auto *label = (Binary *) node;

        if (TranslationUnitJBNew(this->unit_, (String *) label->left) == nullptr)
            return false;

        return this->Compile_((Node *) label->right);
    } else if (AR_TYPEOF(node, type_ast_ret_)) {
        auto *ret = (Unary *) node;

        if (ret->value != nullptr) {
            if (!this->CompileExpression((Node *) ret->value))
                return false;
        } else {
            if (this->PushStatic(NilVal, true, true) < 0)
                return false;
        }

        return this->Emit(OpCodes::RET, 0, nullptr);
    } else if (AR_TYPEOF(node, type_ast_jmp_))
        return this->CompileJump((Unary *) node);
    else if (AR_TYPEOF(node, type_ast_test_))
        return this->CompileTest((Test *) node);
    else if (AR_TYPEOF(node, type_ast_block_))
        return this->CompileBlock((Unary *) node, true);
    else if (AR_TYPEOF(node, type_ast_loop_))
        return this->CompileLoop((Loop *) node);
    else if (AR_TYPEOF(node, type_ast_for_))
        return this->CompileForLoop((Loop *) node);
    else if (AR_TYPEOF(node, type_ast_for_in_))
        return this->CompileForInLoop((Loop *) node);

    ErrorFormat(type_compile_error_, "invalid AST node: %s", AR_TYPE_NAME(node));
    return false;
}

bool Compiler::CompileBlock(Unary *block, bool enter_sub) {
    ArObject *iter;
    ArObject *tmp;

    if (!AR_TYPEOF(block->value, type_list_)) {
        ErrorFormat(type_compile_error_, "unexpected value in ((Unary *)block)->value, expected 'list' not '%s'",
                    AR_TYPE_NAME(block->value));
        return false;
    }

    if (enter_sub && !TranslationUnitEnterSub(this->unit_))
        return false;

    if ((iter = IteratorGet(block->value)) == nullptr) {
        TranslationUnitExitSub(this->unit_);
        return false;
    }

    while ((tmp = IteratorNext(iter)) != nullptr) {
        if (!this->Compile_((Node *) tmp)) {
            TranslationUnitExitSub(this->unit_);
            Release(iter);
            Release(tmp);
            return false;
        }

        Release(tmp);
    }

    Release(iter);

    if (enter_sub)
        TranslationUnitExitSub(this->unit_);

    return true;
}

bool Compiler::CompileCall(Binary *call) {
    auto *left = (Node *) call->left;
    OpCodeCallFlags flags{};
    OpCodes code = OpCodes::CALL;
    int args = 0;

    ArObject *iter;
    ArObject *tmp;

    if (!this->CompileExpression(left))
        return false;

    if (AR_TYPEOF(left, type_ast_selector_) && left->kind != TokenType::SCOPE)
        flags |= OpCodeCallFlags::METHOD;

    if ((iter = IteratorGet(call->right)) == nullptr)
        return false;

    while ((tmp = IteratorNext(iter)) != nullptr) {
        args++;
        if (AR_TYPEOF(tmp, type_ast_spread_)) {
            if (!this->CompileExpression((Node *) ((Unary *) tmp)->value)) {
                Release(tmp);
                Release(iter);
                return false;
            }
            flags |= OpCodeCallFlags::SPREAD;
        } else {
            if (!this->CompileExpression((Node *) tmp)) {
                Release(tmp);
                Release(iter);
                return false;
            }
        }

        Release(tmp);
    }

    Release(iter);

    TranslationUnitDecStack(this->unit_, args);

    if (call->kind == TokenType::DEFER)
        code = OpCodes::DFR;
    else if (call->kind == TokenType::SPAWN)
        code = OpCodes::SPWN;

    if (!this->Emit(code, (unsigned char) flags, args))
        return false;

    return true;
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

bool Compiler::CompileForLoop(Loop *loop) {
    BasicBlock *begin;
    BasicBlock *end;
    JBlock *jb;

    if (!TranslationUnitEnterSub(this->unit_))
        return false;

    if (loop->init != nullptr) {
        if (!this->Compile_(loop->init))
            return false;
    }

    if ((begin = TranslationUnitBlockNew(this->unit_)) == nullptr)
        return false;

    if ((end = BasicBlockNew()) == nullptr)
        return false;

    if ((jb = TranslationUnitJBNewLoop(this->unit_, begin, end)) == nullptr)
        goto ERROR;

    // Compile test

    if (!this->CompileExpression(loop->test))
        goto ERROR;

    if (!this->Emit(OpCodes::JF, end, nullptr))
        goto ERROR;

    // Compile body

    if (!this->Compile_(loop->body))
        goto ERROR;

    // Compile Inc

    if (loop->inc != nullptr) {
        if (!this->CompileExpression(loop->inc))
            goto ERROR;
    }

    if (!this->Emit(OpCodes::JMP, begin, end)) {
        TranslationUnitExitSub(this->unit_);
        return false;
    }

    TranslationUnitJBPop(this->unit_, jb);
    TranslationUnitExitSub(this->unit_);

    return true;

    ERROR:
    BasicBlockDel(end);
    TranslationUnitExitSub(this->unit_);
    return false;
}

bool Compiler::CompileForInLoop(Loop *loop) {
    BasicBlock *end = nullptr;
    BasicBlock *begin;
    JBlock *jb;

    if (!TranslationUnitEnterSub(this->unit_))
        return false;

    if (!this->Compile_(loop->test))
        goto ERROR;

    if (!this->Emit(OpCodes::LDITER, 0, nullptr))
        goto ERROR;

    if ((begin = TranslationUnitBlockNew(this->unit_)) == nullptr)
        goto ERROR;

    if ((end = BasicBlockNew()) == nullptr)
        goto ERROR;

    if ((jb = TranslationUnitJBNewLoop(this->unit_, begin, end)) == nullptr)
        goto ERROR;

    if (!this->Emit(OpCodes::NJE, end, nullptr))
        goto ERROR;

    // ASSIGN
    if (!this->Compile_(loop->init))
        goto ERROR;

    if (!this->Compile_(loop->body))
        goto ERROR;

    if (!this->Emit(OpCodes::JMP, begin, end)) {
        TranslationUnitExitSub(this->unit_);
        return false;
    }

    TranslationUnitJBPop(this->unit_, jb);
    TranslationUnitExitSub(this->unit_);
    TranslationUnitDecStack(this->unit_, 1); // Remove iterator from eval stack

    return true;

    ERROR:
    BasicBlockDel(end);
    TranslationUnitExitSub(this->unit_);
    return false;
}

bool Compiler::CompileLoop(Loop *loop) {
    BasicBlock *begin;
    BasicBlock *end;
    JBlock *jb;

    if ((begin = TranslationUnitBlockNew(this->unit_)) == nullptr)
        return false;

    if ((end = BasicBlockNew()) == nullptr)
        return false;

    if ((jb = TranslationUnitJBNewLoop(this->unit_, begin, end)) == nullptr)
        goto ERROR;

    if (loop->test != nullptr) {
        if (!this->CompileExpression(loop->test))
            goto ERROR;

        if (!this->Emit(OpCodes::JF, end, nullptr))
            goto ERROR;
    }

    if (!TranslationUnitEnterSub(this->unit_))
        goto ERROR;

    if (!this->CompileBlock((Unary *) loop->body, true))
        goto ERROR;

    if (!this->Emit(OpCodes::JMP, begin, end)) {
        TranslationUnitExitSub(this->unit_);
        return false;
    }

    TranslationUnitJBPop(this->unit_, jb);
    TranslationUnitExitSub(this->unit_);

    return true;

    ERROR:
    BasicBlockDel(end);
    TranslationUnitExitSub(this->unit_);
    return false;
}

bool Compiler::CompileTest(Test *test) {
    auto *end = BasicBlockNew();

    if (end == nullptr)
        return false;

    if (!this->CompileExpression((Node *) test->test))
        goto ERROR;

    if (!this->Emit(OpCodes::JF, end, nullptr))
        goto ERROR;

    if (!this->CompileBlock((Unary *) test->body, true))
        goto ERROR;

    if (test->orelse != nullptr) {
        if (!this->Emit(OpCodes::JMP, end, nullptr))
            goto ERROR;

        if (!this->Compile_((Node *) test->orelse))
            goto ERROR;
    }

    TranslationUnitBlockAppend(this->unit_, end);
    return true;

    ERROR:
    BasicBlockDel(end);
    return false;
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

bool Compiler::CompileJump(Unary *jmp) {
    BasicBlock *dst = nullptr;
    JBlock *jb;

    if (jmp->kind == TokenType::BREAK || jmp->kind == TokenType::CONTINUE) {
        if ((jb = TranslationUnitJBFindLoop(this->unit_, (String *) jmp->value)) == nullptr) {
            ErrorFormat(type_compile_error_, "unknown \"loop label\", the loop named '%s' cannot be %s",
                        ((String *) jmp->value)->buffer, jmp->kind == TokenType::BREAK ? "breaked" : "continued");
            return false;
        }

        dst = jb->end;

        if (jmp->kind == TokenType::CONTINUE)
            dst = jb->start;
    }

    return this->Emit(OpCodes::JMP, dst, nullptr);
}

bool Compiler::CompileExpression(Node *expr) {
    if (AR_TYPEOF(expr, type_ast_literal_)) {
        if (!IsHashable(((Unary *) expr)->value))
            return (Code *) ErrorFormat(type_compile_error_, "");

        if (this->PushStatic(((Unary *) expr)->value, true, true) >= 0)
            return true;
    } else if (AR_TYPEOF(expr, type_ast_elvis_)) {
        auto *elvis = (Test *) expr;
        BasicBlock *end;

        if (!this->CompileExpression((Node *) elvis->test))
            return false;

        if ((end = BasicBlockNew()) == nullptr)
            return false;

        if (!this->Emit(OpCodes::JTOP, end, nullptr)) {
            BasicBlockDel(end);
            return false;
        }

        if (!this->CompileExpression((Node *) elvis->orelse)) {
            BasicBlockDel(end);
            return false;
        }

        TranslationUnitBlockAppend(this->unit_, end);
        return true;
    } else if (AR_TYPEOF(expr, type_ast_call_))
        return this->CompileCall((Binary *) expr);
    else if (AR_TYPEOF(expr, type_ast_identifier_))
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
        case OpCodes::NJE:
        case OpCodes::CALL:
        case OpCodes::DFR:
        case OpCodes::SPWN:
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
        case OpCodes::RET:
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

bool Compiler::Emit(argon::lang::OpCodes op, BasicBlock *dest, BasicBlock *next) {
    if (!this->Emit(op, 0, dest))
        return false;

    if (next == nullptr) {
        if (!TranslationUnitBlockNew(this->unit_))
            return false;
    } else
        TranslationUnitBlockAppend(this->unit_, next);

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

    if (this->unit_->scope != TUScope::FUNCTION && sym->nested == 0) {
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
