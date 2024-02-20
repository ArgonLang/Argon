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
        default:
            assert(false);
    }
}

// *********************************************************************************************************************
// EXPRESSION-ZONE
// *********************************************************************************************************************

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

int Compiler::LoadStaticNil(const scanner::Loc *loc, bool emit) {
    return this->LoadStatic((ArObject *) Nil, loc, true, emit);
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
            // TODO CompileFunction
            assert(false);
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
            // TODO CompileSubscr
            assert(false);
        case node::NodeType::INFIX:
            if (node->token_type == scanner::TokenType::AND || node->token_type == scanner::TokenType::OR) {
                this->CompileTest((const node::Binary *) node);
                break;
            }

            this->CompileInfix((const node::Binary *) node);
            break;
        case node::NodeType::OBJ_INIT:
            // TODO CompileINIT
            assert(false);
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
            // TODO CompileTrap
            assert(false);
        case node::NodeType::SAFE_EXPR:
            // TODO CompileSafeExpr
            assert(false);
        case node::NodeType::SELECTOR:
            // TODO CompileSelector
            assert(false);
        case node::NodeType::UPDATE:
            // TODO CompileUpdate
            assert(false);
        default:
            throw CompilerException(kCompilerErrors[1], (int) node->node_type, __FUNCTION__);
    }
}

// *********************************************************************************************************************
// PRIVATE
// *********************************************************************************************************************

void Compiler::EnterScope(String *name, SymbolType type) {
    auto *unit = TranslationUnitNew(this->unit_, name, type);
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
