// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/nil.h>

#include <argon/lang/compiler2/compiler2.h>

using namespace argon::vm::datatype;
using namespace argon::lang::compiler2;
using namespace argon::lang::parser2;

#define CHECK_AST_NODE(expected, chk)                                                       \
do {                                                                                        \
    if(!AR_TYPEOF(chk, expected))                                                           \
        throw CompilerException(kCompilerErrors[0], (expected)->name, AR_TYPE_NAME(chk));   \
} while(0)

SymbolT *Compiler::IdentifierLookupOrCreate(String *id, argon::lang::compiler2::SymbolType type) {
    auto *dst = this->unit_->names;

    auto *sym = this->unit_->symt->SymbolLookup(id);
    if (sym == nullptr) {
        if ((sym = this->unit_->symt->SymbolInsert(id, type)) == nullptr)
            throw DatatypeException();

        if (this->unit_->IsFreeVar(id)) {
            dst = this->unit_->enclosed;
            sym->free = true;
        }

        sym->id = (short) dst->length;

        if (!ListAppend(dst, (ArObject *) id)) {
            Release(sym);

            throw DatatypeException();
        }
    }

    return sym;
}

void Compiler::Compile(const node::Node *node) {
    switch (node->node_type) {
        case node::NodeType::EXPRESSION:
            this->Expression((const node::Node *) ((const node::Unary *) node)->value);
            break;
        case node::NodeType::FUNCTION:
            this->CompileFunction((const node::Function *) node);
            break;
        default:
            assert(false);
    }
}

// *********************************************************************************************************************
// EXPRESSION-ZONE
// *********************************************************************************************************************

int Compiler::CompileSelector(const node::Binary *binary, bool dup, bool emit) {
    const auto *cursor = binary;
    int idx;

    CHECK_AST_NODE(node::type_ast_selector_, binary);

    auto deep = 0;
    while (((const node::Node *) cursor->left)->node_type == node::NodeType::SELECTOR) {
        cursor = (const node::Binary *) cursor->left;
        deep++;
    }

    this->Expression((const node::Node *) cursor->left);

    do {
        vm::OpCode code;

        switch (cursor->token_type) {
            case scanner::TokenType::SCOPE:
                code = vm::OpCode::LDSCOPE;
                break;
            case scanner::TokenType::DOT:
                code = vm::OpCode::LDATTR;
                break;
            case scanner::TokenType::QUESTION_DOT:
                code = vm::OpCode::LDATTR;
                this->unit_->Emit(vm::OpCode::JNIL, 0, this->unit_->jblock->end, &cursor->loc);
                break;
            default:
                throw CompilerException(kCompilerErrors[2], cursor->token_type, __FUNCTION__);
        }

        idx = this->LoadStatic((node::ArObject *) cursor->right, &cursor->loc, true, false);

        if (dup && deep == 0)
            this->unit_->Emit(vm::OpCode::DUP, 1, nullptr, nullptr);

        if (deep > 0 || emit)
            this->unit_->Emit(code, idx, nullptr, &cursor->loc);

        deep--;
        cursor = binary;
        for (int i = 0; i < deep; i++)
            cursor = (const node::Binary *) cursor->left;
    } while (deep >= 0);

    return idx;
}

int Compiler::LoadStatic(ArObject *object, const scanner::Loc *loc, bool store, bool emit) {
    auto *value = IncRef(object);
    ArObject *tmp;

    int idx = -1;

    if (store) {
        // Check if value are already present in TranslationUnit
        if ((tmp = DictLookup(this->unit_->statics_map, value)) == nullptr) {
            // Value not found in the current TranslationUnit, trying in global_statics
            if ((tmp = DictLookup(this->static_globals_, value)) != nullptr) {
                // Recover already existing object and discard the actual one
                Release(value);
                value = tmp;
            } else if (!DictInsert(this->static_globals_, value, value)) {
                Release(value);

                throw DatatypeException();
            }

            auto *index = UIntNew(this->unit_->statics->length);
            if (index == nullptr) {
                Release(value);

                throw DatatypeException();
            }

            if (!DictInsert(this->unit_->statics_map, value, (ArObject *) index)) {
                Release(value);
                Release(index);

                throw DatatypeException();
            }

            Release(index);
        } else {
            idx = (int) ((Integer *) tmp)->sint;
            Release(tmp);
        }
    }

    if (!store || idx == -1) {
        idx = (int) this->unit_->statics->length;
        if (!ListAppend(this->unit_->statics, value)) {
            Release(value);

            throw DatatypeException();
        }
    }

    Release(value);

    if (emit)
        this->unit_->Emit(vm::OpCode::LSTATIC, idx, nullptr, loc);

    return idx;
}

int Compiler::LoadStatic(const node::Unary *literal, bool store, bool emit) {
    CHECK_AST_NODE(node::type_ast_literal_, literal);

    return this->LoadStatic(literal->value, &literal->loc, store, emit);
}

int Compiler::LoadStaticAtom(const char *key, const scanner::Loc *loc, bool emit) {
    ARC atom;

    atom = AtomNew(key);
    if (!atom)
        throw DatatypeException();

    return this->LoadStatic(atom.Get(), loc, false, emit);
}

int Compiler::LoadStaticNil(const scanner::Loc *loc, bool emit) {
    return this->LoadStatic((ArObject *) Nil, loc, true, emit);
}

String *Compiler::MakeFnName() {
    String *name;

    if (this->unit_->name != nullptr)
        name = StringFormat("%s$%d", ARGON_RAW_STRING(this->unit_->name), this->unit_->anon_count++);
    else
        name = StringFormat("$%d", this->unit_->anon_count++);

    if (name == nullptr)
        throw DatatypeException();

    return name;
}

void Compiler::IdentifierNew(String *name, const scanner::Loc *loc, SymbolType type, AttributeFlag aflags, bool emit) {
    ARC sym;

    if (StringEqual(name, "_"))
        throw CompilerException(kCompilerErrors[3], "_");

    sym = (ArObject *) this->unit_->symt->SymbolInsert(name, type);
    if (!sym)
        throw DatatypeException();

    auto *dest = this->unit_->names;
    auto *p_sym = (SymbolT *) sym.Get();

    p_sym->declared = true;

    if (this->unit_->symt->type == SymbolType::STRUCT || this->unit_->symt->type == SymbolType::TRAIT) {
        this->LoadStatic((ArObject *) name, loc, true, true);

        this->unit_->Emit(vm::OpCode::TSTORE, (unsigned char) aflags, nullptr, loc);
        return;
    }

    if (p_sym->nested == 0) {
        auto id = (unsigned short) (p_sym->id >= 0 ? p_sym->id : dest->length);

        if (emit)
            this->unit_->Emit(vm::OpCode::NGV, (unsigned char) aflags, id, loc);

        if (p_sym->id >= 0)
            return;
    } else {
        dest = this->unit_->locals;

        if (emit)
            this->unit_->Emit(vm::OpCode::STLC, (int) dest->length, nullptr, loc);
    }

    ArObject *arname;

    if (p_sym->id >= 0)
        arname = ListGet(!p_sym->free ? this->unit_->names : this->unit_->enclosed, p_sym->id);
    else
        arname = (ArObject *) IncRef(name);

    p_sym->id = (short) dest->length;

    if (!ListAppend(dest, arname)) {
        Release(arname);

        throw DatatypeException();
    }

    Release(arname);
}

void Compiler::IdentifierNew(const node::Unary *id, SymbolType type, AttributeFlag aflags, bool emit) {
    CHECK_AST_NODE(node::type_ast_identifier_, id);

    this->IdentifierNew((String *) id->value, &id->loc, type, aflags, emit);
}

void Compiler::LoadIdentifier(String *identifier, const scanner::Loc *loc) {
    // N.B. Unknown variable, by default does not raise an error,
    // but tries to load it from the global namespace.
    if (StringEqual(identifier, (const char *) "_"))
        throw CompilerException(kCompilerErrors[3], identifier);

    auto *sym = this->IdentifierLookupOrCreate(identifier, SymbolType::VARIABLE);

    auto sym_id = sym->id;
    auto nested = sym->nested;
    auto declared = sym->declared;
    auto free = sym->free;

    Release(sym);

    auto scope = this->unit_->symt->type;
    if (scope != SymbolType::STRUCT
        && scope != SymbolType::TRAIT
        && nested > 0) {
        if (declared) {
            this->unit_->Emit(vm::OpCode::LDLC, sym_id, nullptr, loc);
            return;
        } else if (free) {
            this->unit_->Emit(vm::OpCode::LDENC, sym_id, nullptr, loc);
            return;
        }
    }

    this->unit_->Emit(vm::OpCode::LDGBL, sym_id, nullptr, loc);
}

void Compiler::LoadIdentifier(const node::Unary *identifier) {
    CHECK_AST_NODE(node::type_ast_identifier_, identifier);

    this->LoadIdentifier((String *) identifier->value, &identifier->loc);
}

void Compiler::CompileBlock(const node::Node *node, bool sub) {
    ARC iter;
    ARC stmt;

    CHECK_AST_NODE(node::type_ast_unary_, node);

    iter = IteratorGet(((const node::Unary *) node)->value, false);
    if (!iter)
        throw DatatypeException();

    if (sub && !this->unit_->symt->NewNestedTable())
        throw DatatypeException();

    while ((stmt = IteratorNext(iter.Get())))
        this->Compile((const node::Node *) stmt.Get());

    if (sub)
        SymbolExitNested(this->unit_->symt);
}

void Compiler::CompileDLST(const node::Unary *unary) {
    ARC iter;
    ARC tmp;

    vm::OpCode code;
    int items = 0;

    CHECK_AST_NODE(node::type_ast_unary_, unary);

    iter = IteratorGet(unary->value, false);
    if (!iter)
        throw DatatypeException();

    while ((tmp = IteratorNext(iter.Get()))) {
        this->Expression((node::Node *) tmp.Get());
        items++;
    }

    switch (unary->node_type) {
        case node::NodeType::DICT:
            code = vm::OpCode::MKDT;
            break;
        case node::NodeType::LIST:
            code = vm::OpCode::MKLT;
            break;
        case node::NodeType::SET:
            code = vm::OpCode::MKST;
            break;
        case node::NodeType::TUPLE:
            code = vm::OpCode::MKTP;
            break;
        default:
            throw CompilerException(kCompilerErrors[2], (int) unary->token_type, __FUNCTION__);
    }

    this->unit_->Emit(code, items, nullptr, &unary->loc);
}

void Compiler::CompileElvis(const node::Binary *binary) {
    CHECK_AST_NODE(node::type_ast_binary_, binary);

    this->Expression((node::Node *) binary->left);

    auto *end = BasicBlockNew();
    if (end == nullptr)
        throw DatatypeException();

    try {
        this->unit_->Emit(vm::OpCode::JTOP, 0, end, &binary->loc);

        this->Expression((node::Node *) binary->right);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileFunction(const node::Function *func) {
    ARC code;
    ARC fname;

    FunctionFlags flags{};

    CHECK_AST_NODE(node::type_ast_function_, func);

    if (func->name != nullptr)
        fname = func->name;
    else
        fname = this->MakeFnName();

    if (this->unit_->symt->type == SymbolType::STRUCT
        || this->unit_->symt->type == SymbolType::TRAIT)
        flags = FunctionFlags::STATIC;

    this->EnterScope((String *) fname.Get(), SymbolType::FUNC);

    unsigned short p_count = 0;

    this->CompileFunctionParams(func->params, p_count, flags);

    if (func->body != nullptr)
        this->CompileBlock(func->body, false);
    else
        this->CompileFunctionDefBody(func, (String *) fname.Get());

    if (!this->unit_->bbb.CheckLastInstr(vm::OpCode::RET)) {
        // If the function is empty or the last statement is not return,
        // forcefully enter return as last statement

        if (this->unit_->symt->type == SymbolType::GENERATOR)
            this->LoadStaticAtom("stop", nullptr, true);
        else
            this->LoadStaticNil(nullptr, true);

        this->unit_->Emit(vm::OpCode::RET, nullptr);
    }

    // Update function flags if is a generator
    if (this->unit_->symt->type == SymbolType::GENERATOR)
        flags |= FunctionFlags::GENERATOR;

    // TODO assemble

    this->ExitScope();

    this->CompileFunctionClosure((const Code *) code.Get(), &func->loc, flags);

    this->CompileFunctionDefArgs(func->params, &func->loc, flags);

    this->LoadStatic(code.Get(), &func->loc, false, true);

    if (func->async)
        flags |= FunctionFlags::ASYNC;

    this->unit_->Emit(vm::OpCode::MKFN, (unsigned char) flags, p_count, &func->loc);

    if (func->name != nullptr) {
        auto aflags = AttributeFlag::CONST;

        if (func->pub)
            aflags |= AttributeFlag::PUBLIC;

        this->IdentifierNew(func->name, &func->loc, SymbolType::FUNC, aflags, true);
    }
}

void Compiler::CompileFunctionClosure(const Code *code, const scanner::Loc *loc, FunctionFlags &flags) {
    if (code->enclosed->length == 0) {
        this->unit_->Emit(vm::OpCode::PSHN, loc);
        return;
    }

    for (ArSize i = 0; i < code->enclosed->length; i++)
        this->LoadIdentifier((String *) code->enclosed->objects[i], loc);

    this->unit_->Emit(vm::OpCode::MKLT, (int) code->enclosed->length, nullptr, loc);
}

void Compiler::CompileFunctionDefArgs(List *params, const scanner::Loc *loc, FunctionFlags &flags) {
    ARC iter;
    ARC tmp;

    unsigned short def_count = 0;

    if (params == nullptr) {
        this->unit_->Emit(vm::OpCode::PSHN, loc);

        return;
    }

    iter = IteratorGet((ArObject *) params, false);
    if (!iter)
        throw DatatypeException();

    while ((tmp = IteratorNext(iter.Get()))) {
        const auto *param = (node::Parameter *) tmp.Get();

        CHECK_AST_NODE(node::type_ast_parameter_, param);

        if (param->value != nullptr) {
            this->Expression(param->value);
            def_count++;
        }

        // Sanity check
        if (def_count > 0 && param->node_type == node::NodeType::ARGUMENT && param->value == nullptr)
            throw CompilerException(kCompilerErrors[4]);
    }

    // Build default arguments tuple!
    if (def_count > 0) {
        this->unit_->Emit(vm::OpCode::MKTP, def_count, nullptr, loc);

        flags |= FunctionFlags::DEFARGS;
    } else
        this->unit_->Emit(vm::OpCode::PSHN, nullptr);
}

void Compiler::CompileFunctionDefBody(const node::Function *func, String *name) {
    ARC msg;

    msg = (ArObject *) StringFormat(kNotImplementedError[1], ARGON_RAW_STRING(name));

    this->LoadStatic((ArObject *) type_error_, &func->loc, true, true);

    this->LoadStaticAtom(kNotImplementedError[0], &func->loc, true);

    this->LoadStatic(msg.Get(), &func->loc, false, true);

    this->unit_->Emit(vm::OpCode::CALL, (unsigned char) vm::OpCodeCallMode::FASTCALL, 2, &func->loc);

    this->unit_->Emit(vm::OpCode::PANIC, &func->loc);
}

void Compiler::CompileFunctionParams(List *params, unsigned short &count, FunctionFlags &flags) {
    ARC iter;
    ARC a_param;

    if (params == nullptr)
        return;

    iter = IteratorGet((ArObject *) params, false);
    if (!iter)
        throw DatatypeException();

    while ((a_param = IteratorNext(iter.Get()))) {
        const auto *param = (node::Parameter *) a_param.Get();

        CHECK_AST_NODE(node::type_ast_parameter_, param);

        if (count == 0 && this->unit_->prev != nullptr) {
            auto pscope = this->unit_->prev->symt->type;
            if ((pscope == SymbolType::STRUCT || pscope == SymbolType::TRAIT) && StringEqual(param->id, "self"))
                flags |= FunctionFlags::METHOD;
        }

        this->IdentifierNew(param->id, &param->loc, SymbolType::VARIABLE, {}, false);

        if (param->value == nullptr)
            count++;

        if (param->node_type == node::NodeType::REST) {
            flags |= FunctionFlags::VARIADIC;
            count--;
        } else if (param->node_type == node::NodeType::KWARG) {
            flags |= FunctionFlags::KWARGS;
            count--;
        } else if (param->node_type != node::NodeType::PARAMETER)
            throw CompilerException(kCompilerErrors[1], (int) param->node_type, __FUNCTION__);
    }
}

void Compiler::CompileInfix(const node::Binary *binary) {
    CHECK_AST_NODE(node::type_ast_infix_, binary);

    this->Expression((const node::Node *) binary->left);
    this->Expression((const node::Node *) binary->right);

    switch (binary->token_type) {
        case scanner::TokenType::ARROW_RIGHT:
            this->unit_->Emit(vm::OpCode::PSHC, &binary->loc);
            break;

            // Maths
        case scanner::TokenType::PLUS:
            this->unit_->Emit(vm::OpCode::ADD, &binary->loc);
            break;
        case scanner::TokenType::MINUS:
            this->unit_->Emit(vm::OpCode::SUB, &binary->loc);
            break;
        case scanner::TokenType::ASTERISK:
            this->unit_->Emit(vm::OpCode::MUL, &binary->loc);
            break;
        case scanner::TokenType::SLASH:
            this->unit_->Emit(vm::OpCode::DIV, &binary->loc);
            break;
        case scanner::TokenType::SLASH_SLASH:
            this->unit_->Emit(vm::OpCode::IDIV, &binary->loc);
            break;
        case scanner::TokenType::PERCENT:
            this->unit_->Emit(vm::OpCode::MOD, &binary->loc);
            break;

            // SHIFT
        case scanner::TokenType::SHL:
            this->unit_->Emit(vm::OpCode::SHL, &binary->loc);
            break;
        case scanner::TokenType::SHR:
            this->unit_->Emit(vm::OpCode::SHR, &binary->loc);
            break;

            // EQUALITY
        case scanner::TokenType::EQUAL_EQUAL:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::EQ, nullptr, &binary->loc);
            break;
        case scanner::TokenType::EQUAL_STRICT:
            this->unit_->Emit(vm::OpCode::EQST, (int) CompareMode::EQ, nullptr, &binary->loc);
            break;
        case scanner::TokenType::NOT_EQUAL:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::NE, nullptr, &binary->loc);
            break;
        case scanner::TokenType::NOT_EQUAL_STRICT:
            this->unit_->Emit(vm::OpCode::EQST, (int) CompareMode::NE, nullptr, &binary->loc);
            break;

            // LOGICAL
        case scanner::TokenType::AMPERSAND:
            this->unit_->Emit(vm::OpCode::LAND, &binary->loc);
            break;
        case scanner::TokenType::PIPE:
            this->unit_->Emit(vm::OpCode::LOR, &binary->loc);
            break;
        case scanner::TokenType::CARET:
            this->unit_->Emit(vm::OpCode::LXOR, &binary->loc);
            break;

            // RELATIONAL
        case scanner::TokenType::GREATER:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::GR, nullptr, &binary->loc);
            break;
        case scanner::TokenType::GREATER_EQ:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::GRQ, nullptr, &binary->loc);
            break;
        case scanner::TokenType::LESS:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::LE, nullptr, &binary->loc);
            break;
        case scanner::TokenType::LESS_EQ:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::LEQ, nullptr, &binary->loc);
            break;
        default:
            throw CompilerException(kCompilerErrors[2], (int) binary->token_type, __FUNCTION__);
    }
}

void Compiler::CompileNullCoalescing(const node::Binary *binary) {
    CHECK_AST_NODE(node::type_ast_binary_, binary);

    this->Expression((node::Node *) binary->left);

    auto *end = BasicBlockNew();
    if (end == nullptr)
        throw DatatypeException();

    try {
        this->unit_->Emit(vm::OpCode::JNN, 0, end, &binary->loc);
        this->unit_->EmitPOP();

        this->Expression((node::Node *) binary->right);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileObjInit(const node::ObjectInit *init) {
    ARC iter;
    ARC cursor;

    CHECK_AST_NODE(node::type_ast_objinit_, init);

    this->Expression(init->left);

    if (init->values == nullptr) {
        this->unit_->Emit(vm::OpCode::INIT, (unsigned char) vm::OpCodeInitMode::POSITIONAL, 0, &init->loc);

        return;
    }

    iter = IteratorGet(init->values, false);
    if (!iter)
        throw DatatypeException();

    unsigned char items = 0;
    while ((cursor = IteratorNext(iter.Get()))) {
        const auto *node = (const node::Node *) cursor.Get();

        if (init->as_map) {
            if (items++ & 0x01) {
                this->Expression(node);
                continue;
            }

            CHECK_AST_NODE(node::type_ast_identifier_, node);

            this->LoadStatic((ArObject *) ((const node::Unary *) node)->value, &node->loc, true, true);
        } else {
            items++;

            this->Expression(node);
        }
    }

    this->unit_->Emit(vm::OpCode::INIT,
                      (unsigned char) (init->as_map ? vm::OpCodeInitMode::KWARGS : vm::OpCodeInitMode::POSITIONAL),
                      items, &init->loc);
}

void Compiler::CompilePrefix(const node::Unary *unary) {
    CHECK_AST_NODE(node::type_ast_prefix_, unary);

    this->Expression((const node::Node *) unary->value);

    switch (unary->token_type) {
        case scanner::TokenType::EXCLAMATION:
            this->unit_->Emit(vm::OpCode::NOT, &unary->loc);
            break;
        case scanner::TokenType::MINUS:
            this->unit_->Emit(vm::OpCode::NEG, &unary->loc);
            break;
        case scanner::TokenType::PLUS:
            this->unit_->Emit(vm::OpCode::POS, &unary->loc);
            break;
        case scanner::TokenType::TILDE:
            this->unit_->Emit(vm::OpCode::INV, &unary->loc);
            break;
        default:
            throw CompilerException(kCompilerErrors[2], (int) unary->token_type, __FUNCTION__);
    }
}

void Compiler::CompileSafe(const node::Unary *unary) {
    auto *val = (const node::Node *) unary->value;
    BasicBlock *end;

    CHECK_AST_NODE(node::type_ast_unary_, unary);

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        this->unit_->JBPush(nullptr, nullptr, end, JBlockType::SAFE);

        if (val->node_type == node::NodeType::ASSIGNMENT)
            this->Compile(val);
        else
            this->Expression(val);

        this->unit_->JBPop();

        this->unit_->BlockAppend(end);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }
}

void Compiler::CompileSubscr(const node::Subscript *subscr, bool dup, bool emit) {
    CHECK_AST_NODE(node::type_ast_subscript_, subscr);

    this->Expression(subscr->expression);

    if (subscr->start != nullptr)
        this->Expression(subscr->start);
    else
        this->LoadStaticNil(&subscr->loc, true);

    if (subscr->node_type == node::NodeType::SLICE) {
        if (subscr->stop != nullptr)
            this->Expression(subscr->stop);
        else
            this->LoadStaticNil(&subscr->loc, true);

        this->unit_->Emit(vm::OpCode::MKBND, &subscr->loc);
    }

    if (dup)
        this->unit_->Emit(vm::OpCode::DUP, 2, nullptr, nullptr);

    if (emit)
        this->unit_->Emit(vm::OpCode::SUBSCR, &subscr->loc);
}

void Compiler::CompileTest(const node::Binary *binary) {
    auto *cursor = binary;

    CHECK_AST_NODE(node::type_ast_infix_, binary);

    auto *end = BasicBlockNew();
    if (end == nullptr)
        throw DatatypeException();

    int deep = 0;
    while (((node::Node *) (cursor->left))->token_type == scanner::TokenType::AND
           || ((node::Node *) (cursor->left))->token_type == scanner::TokenType::OR) {
        cursor = (const node::Binary *) cursor->left;
        deep++;
    }

    try {
        this->Expression((node::Node *) cursor->left);

        do {
            if (cursor->token_type == scanner::TokenType::AND)
                this->unit_->Emit(vm::OpCode::JFOP, 0, end, &cursor->loc);
            else if (cursor->token_type == scanner::TokenType::OR)
                this->unit_->Emit(vm::OpCode::JTOP, 0, end, &cursor->loc);
            else
                throw CompilerException(kCompilerErrors[2], (int) cursor->token_type, __FUNCTION__);

            this->unit_->BlockNew();

            this->Expression((node::Node *) cursor->right);

            deep--;

            cursor = binary;
            for (int i = 0; i < deep; i++)
                cursor = (const node::Binary *) cursor->left;
        } while (deep >= 0);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileTernary(const node::Branch *branch) {
    BasicBlock *end;
    BasicBlock *orelse;

    CHECK_AST_NODE(node::type_ast_branch_, branch);

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    if ((orelse = BasicBlockNew()) == nullptr) {
        BasicBlockDel(end);

        throw DatatypeException();
    }

    try {
        this->Expression(branch->test);

        this->unit_->Emit(vm::OpCode::JF, 0, orelse, &branch->test->loc);

        this->Expression(branch->body);

        this->unit_->Emit(vm::OpCode::JMP, 0, end, nullptr);

        this->unit_->DecrementStack(1);

        this->unit_->BlockAppend(orelse);

        if (branch->orelse != nullptr)
            this->Expression(branch->orelse);
        else
            this->LoadStaticNil(&branch->loc, true);
    } catch (...) {
        BasicBlockDel(end);
        BasicBlockDel(orelse);

        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileTrap(const node::Unary *unary) {
    BasicBlock *end;

    CHECK_AST_NODE(node::type_ast_unary_, unary);

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        this->unit_->JBPush(nullptr, nullptr, end, JBlockType::TRAP);

        this->unit_->Emit(vm::OpCode::ST, 0, end, &unary->loc);

        this->Expression((const node::Node *) unary->value);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->JBPop();

    this->unit_->BlockAppend(end);

    this->unit_->Emit(vm::OpCode::POPGT, (int) this->unit_->stack.current, nullptr, nullptr);

    if (this->unit_->CheckBlock(JBlockType::TRAP))
        this->unit_->Emit(vm::OpCode::TRAP, 0, this->unit_->jblock->end, nullptr);
    else
        this->unit_->Emit(vm::OpCode::TRAP, 0, nullptr, nullptr);
}

void Compiler::CompileUpdate(const node::Unary *unary) {
    CHECK_AST_NODE(node::type_ast_update_, unary);

    this->Expression((const node::Node *) unary->value);

    this->unit_->Emit(vm::OpCode::DUP, 1, nullptr, nullptr);

    if (unary->token_type == scanner::TokenType::MINUS_MINUS)
        this->unit_->Emit(vm::OpCode::DEC, &unary->loc);
    else if (unary->token_type == scanner::TokenType::PLUS_PLUS)
        this->unit_->Emit(vm::OpCode::INC, &unary->loc);
    else
        throw CompilerException(kCompilerErrors[2], (int) unary->token_type, __FUNCTION__);

    auto *value = (const node::Unary *) unary->value;

    switch (value->node_type) {
        case node::NodeType::IDENTIFIER:
            this->StoreVariable(value);
            break;
        case node::NodeType::INDEX:
            this->CompileSubscr((const node::Subscript *) value, false, false);

            this->unit_->Emit(vm::OpCode::MTH, 2, nullptr, nullptr);
            this->unit_->Emit(vm::OpCode::STSUBSCR, &value->loc);
            break;
        case node::NodeType::SELECTOR: {
            auto code = vm::OpCode::STATTR;
            if (value->token_type == scanner::TokenType::SCOPE)
                code = vm::OpCode::STSCOPE;

            auto idx = this->CompileSelector((const node::Binary *) value, false, false);

            this->unit_->Emit(vm::OpCode::MTH, 1, nullptr, nullptr);
            this->unit_->Emit(code, idx, nullptr, &value->loc);
            break;
        }
        default:
            throw CompilerException(kCompilerErrors[1], (int) unary->node_type, __FUNCTION__);
    }
}

void Compiler::Expression(const node::Node *node) {
    switch (node->node_type) {
        case node::NodeType::AWAIT:
            CHECK_AST_NODE(node::type_ast_unary_, node);

            this->Expression((const node::Node *) ((const node::Unary *) node)->value);

            this->unit_->Emit(vm::OpCode::AWAIT, &node->loc);
            break;
        case node::NodeType::CALL:
            // TODO CompileCall
            assert(false);
        case node::NodeType::DICT:
        case node::NodeType::LIST:
        case node::NodeType::SET:
        case node::NodeType::TUPLE:
            this->CompileDLST((const node::Unary *) node);
            break;
        case node::NodeType::ELVIS:
            this->CompileElvis((const node::Binary *) node);
            break;
        case node::NodeType::FUNCTION:
            this->CompileFunction((const node::Function *) node);
            break;
        case node::NodeType::IDENTIFIER:
            this->LoadIdentifier((const node::Unary *) node);
            break;
        case node::NodeType::IN:
        case node::NodeType::NOT_IN:
            CHECK_AST_NODE(node::type_ast_binary_, node);

            this->Expression((const node::Node *) ((const node::Binary *) node)->left);
            this->Expression((const node::Node *) ((const node::Binary *) node)->right);

            this->unit_->Emit(vm::OpCode::CNT,
                              (int) (node->node_type == node::NodeType::IN
                                     ? vm::OpCodeContainsMode::IN
                                     : vm::OpCodeContainsMode::NOT_IN),
                              nullptr,
                              &node->loc);
            break;
        case node::NodeType::INDEX:
        case node::NodeType::SLICE:
            this->CompileSubscr((const node::Subscript *) node, false, true);
            break;
        case node::NodeType::INFIX:
            if (node->token_type == scanner::TokenType::AND || node->token_type == scanner::TokenType::OR) {
                this->CompileTest((const node::Binary *) node);
                break;
            }

            this->CompileInfix((const node::Binary *) node);
            break;
        case node::NodeType::OBJ_INIT:
            this->CompileObjInit((const node::ObjectInit *) node);
            break;
        case node::NodeType::LITERAL:
            this->LoadStatic((const node::Unary *) node, true, true);
            break;
        case node::NodeType::NULL_COALESCING:
            this->CompileNullCoalescing((const node::Binary *) node);
            break;
        case node::NodeType::PREFIX:
            this->CompilePrefix((const node::Unary *) node);
            break;
        case node::NodeType::TERNARY:
            this->CompileTernary((const node::Branch *) node);
            break;
        case node::NodeType::TRAP:
            this->CompileTrap((const node::Unary *) node);
            break;
        case node::NodeType::SAFE_EXPR:
            this->CompileSafe((const node::Unary *) node);
            break;
        case node::NodeType::SELECTOR:
            this->CompileSelector((const node::Binary *) node, false, true);
            break;
        case node::NodeType::UPDATE:
            this->CompileUpdate((const node::Unary *) node);
            break;
        default:
            throw CompilerException(kCompilerErrors[1], (int) node->node_type, __FUNCTION__);
    }
}

void Compiler::StoreVariable(String *id, const scanner::Loc *loc) {
    vm::OpCode code = vm::OpCode::STGBL;

    if (StringEqual(id, (const char *) "_")) {
        this->unit_->Emit(vm::OpCode::POP, nullptr);

        return;
    }

    auto *sym = this->IdentifierLookupOrCreate(id, SymbolType::VARIABLE);

    if (sym->declared && (this->unit_->symt->type == SymbolType::FUNC || sym->nested > 0))
        code = vm::OpCode::STLC;
    else if (sym->free)
        code = vm::OpCode::STENC;

    auto sym_id = sym->id;

    Release(sym);

    this->unit_->Emit(code, sym_id, nullptr, loc);
}

void Compiler::StoreVariable(const node::Unary *identifier) {
    CHECK_AST_NODE(node::type_ast_identifier_, identifier);

    return this->StoreVariable((String *) identifier->value, &identifier->loc);
}

// *********************************************************************************************************************
// PRIVATE
// *********************************************************************************************************************

void Compiler::EnterScope(String *name, SymbolType type) {
    auto *unit = TranslationUnitNew(this->unit_, name, type);
    if (unit == nullptr)
        throw DatatypeException();

    this->unit_ = unit;
}

void Compiler::ExitScope() {
    this->unit_ = TranslationUnitDel(this->unit_);
}

// *********************************************************************************************************************
// PUBLIC
// *********************************************************************************************************************

Code *Compiler::Compile(node::Module *mod) {
    ARC decl_iter;
    ArObject *decl;

    // Initialize global_statics
    if (this->static_globals_ == nullptr) {
        this->static_globals_ = DictNew();

        if (this->static_globals_ == nullptr)
            return nullptr;
    }

    try {
        decl_iter = IteratorGet((ArObject *) mod->statements, false);
        if (!decl_iter)
            throw DatatypeException();

        // Let's start creating a new context
        this->EnterScope(mod->filename, SymbolType::MODULE);

        while ((decl = IteratorNext(decl_iter.Get())) != nullptr) {
            this->Compile((node::Node *) decl);

            Release(&decl);
        }

        Release(decl);

        this->ExitScope();
    } catch (CompilerException &e) {
        assert(false);
    }

    return nullptr;
}
