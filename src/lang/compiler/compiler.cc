// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/error.h>
#include <object/datatype/integer.h>
#include <object/datatype/nil.h>
#include <object/datatype/function.h>

#include "basicblock.h"
#include "compiler.h"

using namespace argon::lang::scanner;
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
            if (this->PushStatic(NilVal, true, true) < 0)
                return false;
        } else {
            if (!this->CompileExpression((Unary *) variable->value))
                return false;
        }

        if (!this->IdentifierNew((String *) variable->name, SymbolType::VARIABLE, flags, true))
            return false;

        return true;
    } else if (AR_TYPEOF(node, type_ast_list_decl_))
        return this->CompileDeclarations((Unary *) node);
    else if (AR_TYPEOF(node, type_ast_label_)) {
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
    } else if (AR_TYPEOF(node, type_ast_import_decl_)) {
        if (((ImportDecl *) node)->module != nullptr)
            return this->CompileImportFrom((ImportDecl *) node);
        return this->CompileImport((ImportDecl *) node);
    } else if (AR_TYPEOF(node, type_ast_struct_) || AR_TYPEOF(node, type_ast_trait_))
        return this->CompileConstruct((Construct *) node);
    else if (AR_TYPEOF(node, type_ast_assignment_))
        return this->CompileAssignment((Binary *) node);
    else if (AR_TYPEOF(node, type_ast_call_))
        return this->CompileCall((Binary *) node);
    else if (AR_TYPEOF(node, type_ast_safe_))
        return this->CompileSafe((Unary *) node);
    else if (AR_TYPEOF(node, type_ast_func_))
        return this->CompileFunction((Construct *) node);
    else if (AR_TYPEOF(node, type_ast_jmp_))
        return this->CompileJump((Unary *) node);
    else if (AR_TYPEOF(node, type_ast_test_))
        return this->CompileIf((Test *) node);
    else if (AR_TYPEOF(node, type_ast_block_))
        return this->CompileBlock((Unary *) node, true);
    else if (AR_TYPEOF(node, type_ast_loop_))
        return this->CompileLoop((Loop *) node);
    else if (AR_TYPEOF(node, type_ast_for_))
        return this->CompileForLoop((Loop *) node);
    else if (AR_TYPEOF(node, type_ast_for_in_))
        return this->CompileForInLoop((Loop *) node);
    else if (AR_TYPEOF(node, type_ast_switch_))
        return this->CompileSwitch((Test *) node);

    ErrorFormat(type_compile_error_, "invalid AST node: %s", AR_TYPE_NAME(node));
    return false;
}

bool Compiler::CompileDeclarations(Unary *declist) {
    ArObject *iter;
    Assignment *decl;
    bool ok = true;

    if ((iter = IteratorGet(declist->value)) == nullptr)
        return false;

    while (ok && (decl = (Assignment *) IteratorNext(iter)) != nullptr) {
        ok = this->Compile_(decl);
        Release(decl);
    }

    Release(iter);
    return ok;
}

bool Compiler::CompileAssignment(Binary *assignment) {
    if (assignment->kind != TokenType::EQUAL)
        return this->CompileAugAssignment(assignment);

    if (!this->CompileExpression((Node *) assignment->right))
        return false;

    if (AR_TYPEOF(assignment->left, type_ast_identifier_))
        return this->VariableStore((String *) ((Unary *) assignment->left)->value);
    else if (AR_TYPEOF(assignment->left, type_ast_selector_)) {
        if (this->CompileSelector((Binary *) assignment->left, false, false) < 0)
            return false;

        if (((Binary *) assignment->left)->kind == TokenType::SCOPE)
            return this->Emit(OpCodes::STSCOPE, 0, nullptr);

        return this->Emit(OpCodes::STATTR, 0, nullptr);
    } else if (AR_TYPEOF(assignment->left, type_ast_subscript_) | AR_TYPEOF(assignment->left, type_ast_index_)) {
        if (!this->CompileSubscr((Subscript *) assignment->left, false, false))
            return false;

        return this->Emit(OpCodes::STSUBSCR, 0, nullptr);
    } else if (AR_TYPEOF(assignment->left, type_ast_tuple_))
        return this->CompileUnpack((List *) ((Unary *) assignment->left)->value);

    return false;
}

bool Compiler::CompileAugAssignment(Binary *assignment) {
#define COMPILE_OP()                                                \
        if (!this->CompileExpression((Node *) assignment->right))   \
            return false;                                           \
        if (!this->Emit(opcode, 0, nullptr))                        \
            return false

    OpCodes opcode = OpCodes::IPADD;

    // Select opcode
    switch (assignment->kind) {
        case TokenType::PLUS_EQ:
            opcode = OpCodes::IPADD;
            break;
        case TokenType::MINUS_EQ:
            opcode = OpCodes::IPSUB;
            break;
        case TokenType::ASTERISK_EQ:
            opcode = OpCodes::IPMUL;
            break;
        case TokenType::SLASH_EQ:
            opcode = OpCodes::IPDIV;
            break;
        default:
            assert(false);
    }

    if (AR_TYPEOF(assignment->left, type_ast_identifier_)) {
        if (!this->IdentifierLoad((String *) ((Unary *) assignment->left)->value))
            return false;

        COMPILE_OP();

        return this->VariableStore((String *) ((Unary *) assignment->left)->value);
    } else if (AR_TYPEOF(assignment->left, type_ast_selector_)) {
        if (this->CompileSelector((Binary *) assignment->left, true, true) < 0)
            return false;

        COMPILE_OP();

        if (!this->Emit(OpCodes::PB_HEAD, 1, nullptr))
            return false;

        if (((Binary *) assignment->left)->kind == TokenType::SCOPE)
            return this->Emit(OpCodes::STSCOPE, 0, nullptr);

        return this->Emit(OpCodes::STATTR, 0, nullptr);
    } else if (AR_TYPEOF(assignment->left, type_ast_subscript_)) {
        if (!this->CompileSubscr((Subscript *) assignment->left, true, true))
            return false;

        COMPILE_OP();

        if (!this->Emit(OpCodes::PB_HEAD, 3, nullptr))
            return false;

        return this->Emit(OpCodes::STSUBSCR, 0, nullptr);
    }

    return false;
#undef COMPILE_OP
}

bool Compiler::CompileImportAlias(argon::lang::parser::Binary *alias, bool impfrm) {
    OpCodes code = OpCodes::IMPMOD;
    int idx;

    if (impfrm)
        code = OpCodes::IMPFRM;

    if ((idx = this->PushStatic(alias->left, true, false)) < 0)
        return false;

    if (!this->Emit(code, idx, nullptr))
        return false;

    if (!this->IdentifierNew((String *) alias->right, SymbolType::VARIABLE, PropertyType{}, true))
        return false;

    return true;
}

bool Compiler::CompileImport(ImportDecl *import) {
    ArObject *iter;
    ArObject *tmp;

    if (AR_TYPEOF(import->names, type_ast_import_name_))
        return this->CompileImportAlias((Binary *) import->names, false);

    if ((iter = IteratorGet(import->names)) == nullptr)
        return false;

    while ((tmp = IteratorNext(iter)) != nullptr) {
        if (!this->CompileImportAlias((Binary *) tmp, false)) {
            Release(tmp);
            Release(iter);
            return false;
        }

        Release(tmp);
    }

    Release(iter);
    return true;
}

bool Compiler::CompileImportFrom(ImportDecl *import) {
    ArObject *iter;
    ArObject *tmp;
    int idx;

    if ((idx = this->PushStatic(import->module, true, false)) < 0)
        return false;

    if (!this->Emit(OpCodes::IMPMOD, idx, nullptr))
        return false;

    if (!import->star) {
        if (!AR_TYPEOF(import->names, type_ast_import_name_)) {
            if ((iter = IteratorGet(import->names)) == nullptr)
                return false;

            while ((tmp = IteratorNext(iter)) != nullptr) {
                if (!this->CompileImportAlias((Binary *) tmp, true)) {
                    Release(tmp);
                    Release(iter);
                    return false;
                }

                Release(tmp);
            }

            Release(iter);
        } else {
            if (!this->CompileImportAlias((Binary *) import->names, true))
                return false;
        }
    } else {
        if (!this->Emit(OpCodes::IMPALL, 0, nullptr))
            return false;
    }

    return this->Emit(OpCodes::POP, 0, nullptr);
}

bool Compiler::CompileInit(Binary *init) {
    OpCodeINITFlags flag = OpCodeINITFlags::LIST;
    int items = 0;

    ArObject *iter;
    ArObject *tmp;

    if (!this->CompileExpression((Node *) init->left))
        return false;

    if ((iter = IteratorGet(init->right)) == nullptr)
        return false;

    if (AR_TYPEOF(init, type_ast_kwinit_)) {
        while ((tmp = IteratorNext(iter)) != nullptr) {
            if (items++ & 0x01) {
                if (!this->CompileExpression((Node *) tmp)) {
                    Release(tmp);
                    Release(iter);
                    return false;
                }
            } else {
                if (this->PushStatic((String *) tmp, true, true) < 0) {
                    Release(tmp);
                    Release(iter);
                    return false;
                }
            }

            Release(tmp);
        }
    } else {
        while ((tmp = IteratorNext(iter)) != nullptr) {
            items++;

            if (!this->CompileExpression((Node *) tmp)) {
                Release(tmp);
                Release(iter);
                return false;
            }

            Release(tmp);
        }
    }

    Release(iter);

    TranslationUnitDecStack(this->unit_, items + 1); // +1 init->left

    return this->Emit(OpCodes::INIT, (unsigned char) flag, items);
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

        case TokenType::SHL:
            ok = this->Emit(OpCodes::SHL, 0, nullptr);
            break;
        case TokenType::SHR:
            ok = this->Emit(OpCodes::SHR, 0, nullptr);
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

bool Compiler::CompileCall(Binary *call) {
    auto *left = (Node *) call->left;
    OpCodeCallFlags flags{};
    OpCodes code = OpCodes::CALL;
    int args = 0;

    ArObject *iter;
    ArObject *tmp;

    if (AR_TYPEOF(left, type_ast_selector_) && left->kind != TokenType::SCOPE) {
        if ((args = this->CompileSelector((Binary *) left, false, false)) < 0)
            return false;

        if (!this->Emit(OpCodes::LDMETH, args, nullptr))
            return false;

        args = 1;
        flags |= OpCodeCallFlags::METHOD;
    } else {
        if (!this->CompileExpression(left))
            return false;
    }

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

bool Compiler::CompileCompound(Unary *list) {
    OpCodes code = OpCodes::MK_LIST;
    int items = 0;

    ArObject *iter;
    ArObject *tmp;

    if ((iter = IteratorGet(list->value)) == nullptr)
        return false;


    while ((tmp = IteratorNext(iter)) != nullptr) {
        items++;
        if (!this->CompileExpression((Node *) tmp)) {
            Release(tmp);
            Release(iter);
            return false;
        }

        Release(tmp);
    }

    Release(iter);

    if (AR_TYPEOF(list, type_ast_tuple_))
        code = OpCodes::MK_TUPLE;
    else if (AR_TYPEOF(list, type_ast_map_)) {
        code = OpCodes::MK_MAP;
        items /= 2;
    } else if (AR_TYPEOF(list, type_ast_set_))
        code = OpCodes::MK_SET;

    TranslationUnitDecStack(this->unit_, items);

    return this->Emit(code, items, nullptr);
}

bool Compiler::CompileConstruct(Construct *construct) {
    TUScope scope = TUScope::STRUCT;
    OpCodes opcode = OpCodes::MK_STRUCT;
    Code *code = nullptr;
    int impls = 0;

    ArObject *iter;
    ArObject *tmp;

    if (AR_TYPEOF(construct, type_ast_trait_)) {
        scope = TUScope::TRAIT;
        opcode = OpCodes::MK_TRAIT;
    }

    if (!this->TScopeNew(construct->name, scope))
        return false;

    if (!this->CompileBlock((Unary *) construct->block, false))
        return false;

    if ((code = TranslationUnitAssemble(this->unit_)) == nullptr)
        return false;

    this->TScopeExit();

    if (this->PushStatic(code, false, true) < 0) {
        Release(code);
        return false;
    }

    Release(code);

    // TODO: push qname instead of name
    if (this->PushStatic(construct->name, true, true) < 0)
        return false;

    if (construct->params != nullptr) {
        // Impls
        if ((iter = IteratorGet(construct->params)) == nullptr)
            return false;

        while ((tmp = IteratorNext(iter)) != nullptr) {
            impls++;
            if (!this->CompileExpression((Node *) tmp)) {
                Release(tmp);
                Release(iter);
                return false;
            }

            Release(tmp);
        }

        Release(iter);
    }

    if (!this->Emit(opcode, impls, nullptr))
        return false;

    TranslationUnitDecStack(this->unit_, impls);

    return this->IdentifierNew(construct->name, scope == TUScope::STRUCT ? SymbolType::STRUCT : SymbolType::TRAIT,
                               construct->pub ? PropertyType::PUBLIC : PropertyType{}, true);
}

bool Compiler::CompileFunction(Construct *func) {
    Code *fu_code = nullptr;
    String *fname = IncRef(func->name);
    FunctionFlags fun_flags{};
    short p_count = 0;

    ArObject *iter;
    ArObject *tmp;

    if (fname == nullptr) {
        if ((fname = StringNewFormat("<anonymous_%d>", this->unit_->anon_count++)) == nullptr)
            return false;
    }

    if (!this->TScopeNew(fname, TUScope::FUNCTION)) {
        Release(fname);
        return false;
    }

    // Push self as first param in method definition
    if (func->name != nullptr && this->unit_->prev != nullptr) {
        if (this->unit_->prev->scope == TUScope::STRUCT || this->unit_->prev->scope == TUScope::TRAIT) {
            if (!this->IdentifierNew("self", SymbolType::VARIABLE, PropertyType{}, false)) {
                Release(fname);
                return false;
            }

            fun_flags |= FunctionFlags::METHOD;
            p_count++;
        }
    }

    // Store params name
    if (func->params != nullptr) {
        if ((iter = IteratorGet(func->params)) == nullptr) {
            Release(fname);
            return false;
        }

        while ((tmp = IteratorNext(iter)) != nullptr) {
            if (!this->IdentifierNew((String *) ((Unary *) tmp)->value, SymbolType::VARIABLE, PropertyType{}, false)) {
                Release(tmp);
                Release(iter);
                Release(fname);
                return false;
            }

            p_count++;

            if (AR_TYPEOF(tmp, type_ast_restid_)) {
                fun_flags |= FunctionFlags::VARIADIC;
                p_count--;
            }

            Release(tmp);
        }

        Release(iter);
    }

    if (!this->CompileBlock((Unary *) func->block, false)) {
        Release(fname);
        return false;
    }

    // If the function is empty or the last statement is not return,
    // forcefully enter return as last statement
    if (this->unit_->bb.cur->instr.tail == nullptr
        || this->unit_->bb.cur->instr.tail->opcode != (unsigned char) OpCodes::RET) {

        if (this->PushStatic(NilVal, true, true) < 0) {
            Release(fname);
            return false;
        }

        if (!this->Emit(OpCodes::RET, 0, nullptr)) {
            Release(fname);
            return false;
        }
    }

    if ((fu_code = TranslationUnitAssemble(this->unit_)) == nullptr)
        return false;

    this->TScopeExit();

    // TODO: push qname instead of name
    if (this->PushStatic(fname, true, true) < 0) {
        Release(fname);
        return false;
    }

    Release(fname);

    if (this->PushStatic(fu_code, false, true) < 0) {
        Release(fu_code);
        return false;
    }

    // Load closure
    if (fu_code->enclosed->len > 0) {
        for (ArSize i = 0; i < fu_code->enclosed->len; i++) {
            if (!this->IdentifierLoad((String *) fu_code->enclosed->objects[i])) {
                Release(fu_code);
                return false;
            }
        }
        TranslationUnitDecStack(this->unit_, fu_code->enclosed->len);

        if (!this->Emit(OpCodes::MK_LIST, fu_code->enclosed->len, nullptr)) {
            Release(fu_code);
            return false;
        }

        fun_flags |= FunctionFlags::CLOSURE;

        TranslationUnitDecStack(this->unit_, 1);
    }

    Release(fu_code);

    if (!this->Emit(OpCodes::MK_FUNC, (unsigned char) fun_flags, p_count))
        return false;

    if (func->name != nullptr)
        return this->IdentifierNew(func->name, SymbolType::FUNC,
                                   func->pub ? PropertyType::PUBLIC : PropertyType{}, true);

    return true;
}

int Compiler::CompileSelector(Binary *selector, bool dup, bool emit) {
    Binary *cursor = selector;
    int deep = 0;
    int idx;

    OpCodes code;

    while (AR_TYPEOF(cursor->left, type_ast_selector_)) {
        cursor = (Binary *) cursor->left;
        deep++;
    }

    if (!this->CompileExpression((Node *) cursor->left))
        return -1;

    do {
        if (cursor->kind == TokenType::SCOPE)
            code = OpCodes::LDSCOPE;
        else if (cursor->kind == TokenType::DOT)
            code = OpCodes::LDATTR;
        else if (cursor->kind == TokenType::QUESTION_DOT) {
            if (!this->Emit(OpCodes::JNIL, this->unit_->jstack->end, nullptr))
                return false;
        }

        if ((idx = this->PushStatic((String *) cursor->right, true, false)) < 0)
            return -1;

        if (deep > 0 || emit) {
            if (!this->Emit(code, idx, nullptr))
                return -1;
        }

        deep--;
        cursor = selector;
        for (int i = 0; i < deep; i++)
            cursor = (Binary *) selector->left;
    } while (deep >= 0);

    if (dup) {
        if (!this->Emit(OpCodes::DUP, 1, nullptr))
            return -1;

        TranslationUnitIncStack(this->unit_, 1);
    }

    return idx;
}

bool Compiler::CompileSafe(Unary *safe) {
    BasicBlock *end;
    JBlock *jb;
    bool ok;

    if ((end = BasicBlockNew()) == nullptr)
        return false;

    if ((jb = TranslationUnitJBNew(this->unit_, nullptr, end)) == nullptr) {
        BasicBlockDel(end);
        return false;
    }

    ok = AR_TYPEOF(safe->value, type_ast_assignment_) ?
         this->Compile_((Node *) safe->value) : this->CompileExpression((Node *) safe->value);

    if (!ok) {
        BasicBlockDel(end);
        return false;
    }

    TranslationUnitBlockAppend(this->unit_, end);
    TranslationUnitJBPop(this->unit_, jb);
    return true;
}

bool Compiler::CompileSubscr(Subscript *subscr, bool dup, bool emit) {
    if (!this->CompileExpression((Node *) subscr->left))
        return false;

    if (subscr->low != nullptr) {
        if (!this->CompileExpression((Node *) subscr->low))
            return false;
    } else {
        if (!this->PushStatic(NilVal, true, true))
            return false;
    }

    if (AR_TYPEOF(subscr, type_ast_subscript_)) {
        if (subscr->high != nullptr) {
            if (!this->CompileExpression((Node *) subscr->high))
                return false;
        } else {
            if (!this->PushStatic(NilVal, true, true))
                return false;
        }

        if (subscr->high != nullptr) {
            if (!this->CompileExpression((Node *) subscr->high))
                return false;
        } else {
            if (!this->PushStatic(NilVal, true, true))
                return false;
        }

        TranslationUnitDecStack(this->unit_, 3);

        if (!this->Emit(OpCodes::MK_BOUNDS, 3, nullptr))
            return false;
    }

    if (dup) {
        if (!this->Emit(OpCodes::DUP, 2, nullptr))
            return false;
        TranslationUnitIncStack(this->unit_, 2);
    }

    return !emit || this->Emit(OpCodes::SUBSCR, 0, nullptr);
}

bool Compiler::CompileSwitch(Test *sw) {
    ArObject *iter = nullptr;
    Binary *tmp = nullptr;

    BasicBlock *ltest = nullptr;
    BasicBlock *lbody = nullptr;
    BasicBlock *_default = nullptr;
    BasicBlock *tests;
    BasicBlock *bodies;
    BasicBlock *end;

    bool as_if = true;

    if ((tests = TranslationUnitBlockNew(this->unit_)) == nullptr)
        return false;

    if ((bodies = BasicBlockNew()) == nullptr)
        return false;

    if ((end = BasicBlockNew()) == nullptr) {
        BasicBlockDel(bodies);
        return false;
    }

    ltest = tests;
    lbody = bodies;

    if (sw->test != nullptr) {
        if (!this->CompileExpression((Node *) sw->test))
            goto ERROR;

        as_if = false;
    }

    if ((iter = IteratorGet(sw->body)) == nullptr)
        return false;

    while ((tmp = (Binary *) IteratorNext(iter)) != nullptr) {
        if (!this->CompileSwitchCase(tmp, &ltest, &lbody, end, as_if))
            return false;

        if (tmp->left == nullptr && _default == nullptr)
            _default = lbody;

        // Switch to test thread
        this->unit_->bb.cur = ltest;

        Release(tmp);
    }

    Release(iter);

    // End of test thread
    if (!as_if) {
        if (!this->Emit(OpCodes::POP, 0, nullptr))
            goto ERROR;
    }

    if (!this->Emit(OpCodes::JMP, 0, _default == nullptr ? end : _default))
        goto ERROR;

    // Set test bodies

    // *** Remove last useless jmp instruction ***
    for (Instr *cursor = lbody->instr.head; cursor != nullptr; cursor = cursor->next) {
        if (cursor->next != nullptr && cursor->next->jmp == end) {
            argon::memory::Free(cursor->next);
            cursor->next = nullptr;
            lbody->instr.tail = cursor;
            lbody->i_size -= 4;
            break;
        }
    }
    // *******************************************

    TranslationUnitBlockAppend(this->unit_, bodies);
    this->unit_->bb.cur = lbody;

    // Set end block
    TranslationUnitBlockAppend(this->unit_, end);
    return true;

    ERROR:
    Release(iter);
    Release(tmp);
    while ((bodies = BasicBlockDel(bodies)) != nullptr);
    BasicBlockDel(end);
    this->unit_->bb.cur = ltest;
    return false;
}

bool Compiler::CompileSwitchCase(Binary *binary, BasicBlock **ltest, BasicBlock **lbody, BasicBlock *end, bool as_if) {
    bool fallthrough = false;
    BasicBlock *test_curr = *ltest;
    ArObject *iter;
    Node *tmp;

    if (binary->left != nullptr) {
        if ((iter = IteratorGet(binary->left)) == nullptr)
            return false;

        while ((tmp = (Node *) IteratorNext(iter)) != nullptr) {
            if (!this->CompileExpression(tmp))
                goto ERROR;

            if (!as_if) {
                if (!this->Emit(OpCodes::TEST, 0, nullptr))
                    goto ERROR;
            }

            if (!this->Emit(OpCodes::JT, 0, *lbody))
                goto ERROR;

            // Save last test block
            if ((*ltest = TranslationUnitBlockNew(this->unit_)) == nullptr)
                goto ERROR;

            Release(tmp);
        }

        Release(iter);
    }

    // Switch to bodies thread
    this->unit_->bb.cur = *lbody;
    if ((*lbody)->i_size > 0) {
        if (TranslationUnitBlockNew(this->unit_) == nullptr)
            return false;

        if (binary->left != nullptr)
            test_curr->instr.tail->jmp = this->unit_->bb.cur; // Adjust jump to correct block
    }

    // Process body
    if (binary->right != nullptr) {
        if ((iter = IteratorGet(binary->right)) == nullptr)
            return false;

        while ((tmp = (Node *) IteratorNext(iter)) != nullptr) {
            if (!AR_TYPEOF(tmp, type_ast_jmp_) || tmp->kind != TokenType::FALLTHROUGH) {
                fallthrough = false;
                if (!this->Compile_(tmp))
                    goto ERROR;
            } else
                fallthrough = true;

            Release(tmp);
        }

        Release(iter);
    }

    if (!fallthrough) {
        if (!this->Emit(OpCodes::JMP, 0, end))
            return false;
    }

    *lbody = this->unit_->bb.cur;
    return true;

    ERROR:
    Release(iter);
    Release(tmp);
    return false;
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

    if (!this->CompileBlock((Unary *) loop->body, false))
        goto ERROR;

    // Compile Inc

    if (loop->inc != nullptr) {
        if (!this->Compile_(loop->inc))
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

    if (!this->CompileExpression(loop->test))
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
    if (AR_TYPEOF(loop->init, type_ast_identifier_)) {
        if (!this->VariableStore((String *) ((Unary *) loop->init)->value))
            goto ERROR;
    } else if (AR_TYPEOF(loop->init, type_ast_tuple_)) {
        if (!this->CompileUnpack((List *) ((Unary *) loop->init)->value))
            goto ERROR;
    }

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

bool Compiler::CompileIf(Test *test) {
    BasicBlock *orelse;
    BasicBlock *end;

    if ((end = BasicBlockNew()) == nullptr)
        return false;

    orelse = end;

    if (!this->CompileExpression((Node *) test->test))
        goto ERROR;

    if (!this->Emit(OpCodes::JF, orelse, nullptr))
        goto ERROR;

    if (!this->CompileBlock((Unary *) test->body, true))
        goto ERROR;

    if (test->orelse != nullptr) {
        if ((end = BasicBlockNew()) == nullptr) {
            BasicBlockDel(orelse);
            return false;
        }

        if (!this->Emit(OpCodes::JMP, end, orelse)) {
            BasicBlockDel(orelse);
            goto ERROR;
        }

        if (!this->Compile_((Node *) test->orelse))
            goto ERROR;
    }

    TranslationUnitBlockAppend(this->unit_, end);
    return true;

    ERROR:
    BasicBlockDel(end);
    return false;
}

bool Compiler::CompileTest(argon::lang::parser::Binary *test) {
    Binary *cursor = test;
    BasicBlock *end;
    int deep = 0;

    if ((end = BasicBlockNew()) == nullptr)
        return false;

    while (((Node *) cursor->left)->kind == TokenType::AND || ((Node *) cursor->left)->kind == TokenType::OR) {
        cursor = (Binary *) cursor->left;
        deep++;
    }

    if (!this->CompileExpression((Node *) cursor->left))
        goto ERROR;

    do {
        if (cursor->kind == TokenType::AND) {
            if (!this->Emit(OpCodes::JFOP, 0, end))
                goto ERROR;
        } else if (cursor->kind == TokenType::OR) {
            if (!this->Emit(OpCodes::JTOP, 0, end))
                goto ERROR;
        } else
            assert(false);

        if (!TranslationUnitBlockNew(this->unit_))
            goto ERROR;

        if (!this->CompileExpression((Node *) cursor->right))
            goto ERROR;

        deep--;
        cursor = test;
        for (int i = 0; i < deep; i++)
            cursor = (Binary *) test->left;
    } while (deep >= 0);

    TranslationUnitBlockAppend(this->unit_, end);

    return true;

    ERROR:
    BasicBlockDel(end);
    return false;
}

bool Compiler::CompileTernary(Test *ternary) {
    BasicBlock *orelse;
    BasicBlock *end;

    if ((orelse = BasicBlockNew()) == nullptr)
        return false;

    if ((end = BasicBlockNew()) == nullptr) {
        BasicBlockDel(orelse);
        return false;
    }

    if (!this->CompileExpression((Node *) ternary->test))
        goto ERROR;

    if (!this->Emit(OpCodes::JF, orelse, nullptr))
        goto ERROR;

    if (!this->CompileExpression((Unary *) ternary->body))
        goto ERROR;

    if (!this->Emit(OpCodes::JMP, end, nullptr))
        goto ERROR;

    TranslationUnitDecStack(this->unit_, 1);

    TranslationUnitBlockAppend(this->unit_, orelse);

    if (!this->CompileExpression((Node *) ternary->orelse)) {
        BasicBlockDel(end);
        return false;
    }

    TranslationUnitBlockAppend(this->unit_, end);
    return true;

    ERROR:
    BasicBlockDel(orelse);
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
            assert(false);
    }

    return ok;
}

bool Compiler::CompileUpdate(UpdateIncDec *update) {
    OpCodes code = OpCodes::INC;
    int idx;

    if (update->kind == TokenType::MINUS_MINUS)
        code = OpCodes::DEC;

    if (AR_TYPEOF(update->value, type_ast_identifier_)) {
        if (!this->IdentifierLoad((String *) ((Unary *) update->value)->value))
            return false;
    } else if (AR_TYPEOF(update->value, type_ast_index_)) {
        if (!this->CompileSubscr((Subscript *) update->value, true, true))
            return false;
    } else if (AR_TYPEOF(update->value, type_ast_selector_)) {
        if ((idx = this->CompileSelector((Binary *) update->value, true, true)) < 0)
            return false;
    }

    if (update->prefix) {
        if (!this->Emit(code, 0, nullptr))
            return false;
    }

    if (!this->Emit(OpCodes::DUP, 1, nullptr))
        return false;

    TranslationUnitIncStack(this->unit_, 1);

    if (!update->prefix) {
        if (!this->Emit(code, 0, nullptr))
            return false;
    }

    if (AR_TYPEOF(update->value, type_ast_identifier_)) {
        if (!this->VariableStore((String *) ((Unary *) update->value)->value))
            return false;
    } else if (AR_TYPEOF(update->value, type_ast_index_)) {
        if (!this->Emit(OpCodes::PB_HEAD, 3, nullptr))
            return false;

        if (!this->Emit(OpCodes::PB_HEAD, 3, nullptr))
            return false;

        if (!this->Emit(OpCodes::STSUBSCR, 0, nullptr))
            return false;
    } else if (AR_TYPEOF(update->value, type_ast_selector_)) {
        if (!this->Emit(OpCodes::PB_HEAD, 2, nullptr))
            return false;

        if (!this->Emit(OpCodes::PB_HEAD, 2, nullptr))
            return false;

        code = OpCodes::STATTR;
        if (((Node *) update->value)->kind == TokenType::SCOPE)
            code = OpCodes::STSCOPE;

        if (!this->Emit(code, idx, nullptr))
            return false;
    }

    return true;
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
    } else if (AR_TYPEOF(expr, type_ast_list_)
               || AR_TYPEOF(expr, type_ast_tuple_)
               || AR_TYPEOF(expr, type_ast_map_)
               || AR_TYPEOF(expr, type_ast_set_))
        return this->CompileCompound((Unary *) expr);
    else if (AR_TYPEOF(expr, type_ast_init_) || AR_TYPEOF(expr, type_ast_kwinit_))
        return this->CompileInit((Binary *) expr);
    else if (AR_TYPEOF(expr, type_ast_selector_))
        return this->CompileSelector((Binary *) expr, false, true) >= 0;
    else if (AR_TYPEOF(expr, type_ast_func_))
        return this->CompileFunction((Construct *) expr);
    else if (AR_TYPEOF(expr, type_ast_call_))
        return this->CompileCall((Binary *) expr);
    else if (AR_TYPEOF(expr, type_ast_identifier_))
        return this->IdentifierLoad((String *) ((Unary *) expr)->value);
    else if (AR_TYPEOF(expr, type_ast_binary_)) {
        if (expr->kind == TokenType::AND || expr->kind == TokenType::OR)
            return this->CompileTest((Binary *) expr);

        return this->CompileBinary((Binary *) expr);
    } else if (AR_TYPEOF(expr, type_ast_unary_))
        return this->CompileUnary((Unary *) expr);
    else if (AR_TYPEOF(expr, type_ast_update_))
        return this->CompileUpdate((UpdateIncDec *) expr);
    else if (AR_TYPEOF(expr, type_ast_safe_))
        return this->CompileSafe((Unary *) expr);
    else if (AR_TYPEOF(expr, type_ast_index_) || AR_TYPEOF(expr, type_ast_subscript_))
        return this->CompileSubscr((Subscript *) expr, false, true);
    else if (AR_TYPEOF(expr, type_ast_ternary_))
        return this->CompileTernary((Test *) expr);

    return false;
}

bool Compiler::CompileUnpack(List *list) {
    Instr *iptr;
    ArObject *iter;
    ArObject *tmp;

    int items = 0;

    if (!this->Emit(OpCodes::UNPACK, 0, nullptr))
        return false;

    iptr = this->unit_->bb.cur->instr.tail;

    if ((iter = IteratorGet(list)) == nullptr)
        return false;

    while ((tmp = IteratorNext(iter)) != nullptr) {
        TranslationUnitIncStack(this->unit_, 1);
        if (!this->VariableStore((String *) ((Unary *) tmp)->value)) {
            Release(tmp);
            Release(iter);
            return false;
        }
        Release(tmp);
        items++;
    }

    Release(iter);

    InstrSetArg(iptr, items);

    return true;
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
                tmp = nullptr;
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
        case OpCodes::LDMETH:
        case OpCodes::NJE:
        case OpCodes::MK_LIST:
        case OpCodes::MK_TUPLE:
        case OpCodes::MK_SET:
        case OpCodes::MK_MAP:
        case OpCodes::MK_BOUNDS:
        case OpCodes::INIT:
        case OpCodes::IMPMOD:
        case OpCodes::IMPFRM:
            TranslationUnitIncStack(this->unit_, 1);
            break;
        case OpCodes::ADD:
        case OpCodes::IPADD:
        case OpCodes::SUB:
        case OpCodes::IPSUB:
        case OpCodes::MUL:
        case OpCodes::IPMUL:
        case OpCodes::DIV:
        case OpCodes::IPDIV:
        case OpCodes::IDIV:
        case OpCodes::SHL:
        case OpCodes::SHR:
        case OpCodes::LAND:
        case OpCodes::LXOR:
        case OpCodes::LOR:
        case OpCodes::MOD:
        case OpCodes::CMP:
        case OpCodes::POP:
        case OpCodes::JF:
        case OpCodes::JT:
        case OpCodes::NGV:
        case OpCodes::STGBL:
        case OpCodes::STLC:
        case OpCodes::STENC:
        case OpCodes::RET:
        case OpCodes::SUBSCR:
        case OpCodes::UNPACK:
        case OpCodes::MK_FUNC:
        case OpCodes::MK_STRUCT:
        case OpCodes::MK_TRAIT:
        case OpCodes::DFR:
        case OpCodes::SPWN:
        case OpCodes::JFOP:
        case OpCodes::JTOP:
            TranslationUnitDecStack(this->unit_, 1);
            break;
        case OpCodes::STSCOPE:
        case OpCodes::STATTR:
            TranslationUnitDecStack(this->unit_, 2);
            break;
        case OpCodes::STSUBSCR:
            TranslationUnitDecStack(this->unit_, 3);
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

    if (StringEq(name, (const unsigned char *) "_", 1)) {
        ErrorFormat(type_compile_error_, "cannot use '_' as value");
        return false;
    }

    if ((sym = this->IdentifierLookupOrCreate(name, SymbolType::VARIABLE)) == nullptr)
        return false;

    if ((this->unit_->scope != TUScope::STRUCT && this->unit_->scope != TUScope::TRAIT) && sym->nested > 0) {
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

bool Compiler::VariableStore(String *name) {
    OpCodes code = OpCodes::STGBL;
    Symbol *sym;
    bool ok;

    if (StringEq(name, (const unsigned char *) "_", 1))
        return this->Emit(OpCodes::POP, 0, nullptr);

    if ((sym = this->IdentifierLookupOrCreate(name, SymbolType::VARIABLE)) == nullptr)
        return false;

    if (sym->declared && (this->unit_->scope == TUScope::FUNCTION || sym->nested > 0))
        code = OpCodes::STLC;
    else if (sym->free)
        code = OpCodes::STENC;

    ok = this->Emit(code, sym->id, nullptr);

    Release(sym);
    return ok;
}

bool Compiler::IdentifierNew(String *name, SymbolType stype, PropertyType ptype, bool emit) {
    List *dest = this->unit_->names;
    ArObject *arname;
    Symbol *sym;
    bool inserted;

    if (StringEq(name, (const unsigned char *) "_", 1)) {
        ErrorFormat(type_compile_error_, "cannot use '_' as name of identifier");
        return false;
    }

    if ((sym = SymbolTableInsert(this->unit_->symt, name, stype, &inserted)) == nullptr)
        return false;

    sym->declared = true;

    if ((this->unit_->scope == TUScope::TRAIT || this->unit_->scope == TUScope::STRUCT || sym->nested == 0)) {
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

bool Compiler::IdentifierNew(const char *name, SymbolType stype, PropertyType ptype, bool emit) {
    String *aname;
    bool ok;

    if ((aname = StringIntern(name)) == nullptr)
        return false;

    ok = this->IdentifierNew(aname, stype, ptype, emit);

    Release(aname);

    return ok;
}

bool Compiler::TScopeNew(String *name, TUScope scope) {
    SymbolTable *table = this->symt;
    Symbol *symbol;
    String *mangled;
    SymbolType sym_kind;
    TranslationUnit *unit;

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

        if ((mangled = StringNewFormat("%s_symt_", name->buffer)) == nullptr)
            return false;

        if ((symbol = SymbolTableInsertNs(this->unit_->symt, mangled, sym_kind)) == nullptr) {
            Release(mangled);
            return false;
        }

        Release(mangled);
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
    if (this->unit_ != nullptr) {
        assert(this->unit_->stack.current == 0);
        this->unit_ = TranslationUnitDel(this->unit_);
    }
}

Code *Compiler::Compile(File *node) {
    ArObject *decl_iter;
    ArObject *decl;
    Code *code;

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

    code = TranslationUnitAssemble(this->unit_);

    this->TScopeExit();

    return code;
}

Compiler::~Compiler() {
    while ((this->unit_ = TranslationUnitDel(this->unit_)) != nullptr);
    Release(this->symt);
    Release(this->statics_globals_);
}
