// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/opcode.h>

#include <vm/datatype/integer.h>
#include <vm/datatype/nil.h>

#include <lang/scanner/token.h>

#include "compilererr.h"
#include "compiler.h"

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

SymbolT *Compiler::IdentifierLookupOrCreate(String *name, SymbolType type) {
    List *dst = this->unit_->names;

    SymbolT *sym;
    if ((sym = SymbolLookup(this->unit_->symt, name)) == nullptr) {
        if ((sym = SymbolInsert(this->unit_->symt, name, nullptr, type)) == nullptr)
            return nullptr;

        if (this->unit_->IsFreeVar(name)) {
            dst = this->unit_->enclosed;
            sym->free = true;
        }

        sym->id = (unsigned int) dst->length;

        if (!ListAppend(dst, (ArObject *) name)) {
            Release(sym);
            return nullptr;
        }
    }

    return sym;
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

void Compiler::CompileElvis(const parser::Test *test) {
    BasicBlock *end;

    this->Expression(test->test);

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        this->unit_->Emit(vm::OpCode::JTOP, 0, end, &test->loc);

        this->Expression(test->orelse);
    } catch (const std::exception &) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileInit(const parser::Initialization *init) {
    vm::OpCodeInitMode mode = init->as_map ? vm::OpCodeInitMode::KWARGS : vm::OpCodeInitMode::POSITIONAL;
    ARC iter;
    ARC tmp;

    this->Expression(init->left);

    iter = IteratorGet(init->values, false);
    if (!iter)
        throw DatatypeException();

    unsigned char items = 0;
    while ((tmp = IteratorNext(iter.Get()))) {
        const auto *node = (const Node *) tmp.Get();

        if (init->as_map) {
            if (items++ & 0x01) {
                this->Expression(node);
                continue;
            }

            // TODO: check node
            this->LoadStatic(((const Unary *) node)->value, true, true);
            continue;
        }

        items++;
        this->Expression(node);
    }

    this->unit_->DecrementStack(items + 1); // +1 init->left

    this->unit_->Emit(vm::OpCode::INIT, (unsigned char) mode, items, &init->loc);
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

void Compiler::CompileSafe(const parser::Unary *unary) {
    BasicBlock *end;
    const JBlock *jb;

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        jb = this->unit_->JBNew(nullptr, end);

        if (((Node *) unary->value)->node_type == parser::NodeType::ASSIGNMENT)
            this->Compile((const Node *) unary->value);
        else
            this->Expression((const Node *) unary->value);

        this->unit_->BlockAppend(end);

        this->unit_->JBPop(jb);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }
}

void Compiler::CompileSelector(const parser::Binary *selector, bool dup, bool emit) {
    const auto *cursor = selector;
    vm::OpCode code;
    int deep;
    int idx;

    deep = 0;
    while (cursor->left->node_type == parser::NodeType::SELECTOR) {
        cursor = (const parser::Binary *) cursor->left;
        deep++;
    }

    this->Expression(cursor->left);

    do {
        if (cursor->token_type == scanner::TokenType::SCOPE)
            code = vm::OpCode::LDSCOPE;
        else if (cursor->token_type == scanner::TokenType::DOT)
            code = vm::OpCode::LDATTR;
        else if (cursor->token_type == scanner::TokenType::QUESTION_DOT) {
            code = vm::OpCode::LDATTR;
            this->unit_->Emit(vm::OpCode::JNIL, this->unit_->jstack->end, nullptr);
        } else
            throw CompilerException("unexpected TokenType in selector expression");

        idx = this->LoadStatic(((const Unary *) cursor->right)->value, true, false);

        if (dup && deep == 0) {
            this->unit_->Emit(vm::OpCode::DUP, 1, nullptr, nullptr);

            this->unit_->IncrementStack(1);
        }

        if (deep > 0 || emit)
            this->unit_->Emit(code, idx, nullptr, &cursor->loc);

        deep--;
        cursor = selector;
        for (int i = 0; i < deep; i++)
            cursor = (const parser::Binary *) selector->left;
    } while (deep >= 0);
}

void Compiler::CompileSubscr(const parser::Subscript *subscr, bool dup, bool emit) {
    this->Expression(subscr->expression);

    if (subscr->start != nullptr)
        this->Expression(subscr->start);
    else
        this->LoadStatic((ArObject *) Nil, true, true);

    if (subscr->node_type == parser::NodeType::SLICE) {
        if (subscr->stop != nullptr)
            this->Expression(subscr->stop);
        else
            this->LoadStatic((ArObject *) Nil, true, true);

        this->unit_->Emit(vm::OpCode::MKBND, nullptr);
    }

    if (dup) {
        this->unit_->Emit(vm::OpCode::DUP, 2, nullptr, nullptr);
        this->unit_->IncrementStack(2);
    }

    if (emit)
        this->unit_->Emit(vm::OpCode::SUBSCR, &subscr->loc);
}

void Compiler::CompileTernary(const parser::Test *test) {
    BasicBlock *orelse;
    BasicBlock *end;

    if ((orelse = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    if ((end = BasicBlockNew()) == nullptr) {
        BasicBlockDel(orelse);
        throw DatatypeException();
    }

    try {
        this->Expression(test->test);

        this->unit_->Emit(vm::OpCode::JF, orelse, &test->test->loc);

        this->Expression(test->body);

        this->unit_->Emit(vm::OpCode::JMP, end, nullptr);

        this->unit_->DecrementStack(1);

        this->unit_->BlockAppend(orelse);

        if (test->orelse != nullptr)
            this->Expression(test->orelse);
        else
            this->LoadStatic((ArObject *) Nil, true, true);

        this->unit_->BlockAppend(end);
    } catch (...) {
        BasicBlockDel(orelse);
        BasicBlockDel(end);
        throw;
    }
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

void Compiler::CompileUpdate(const parser::Unary *update) {
    const auto *value = (const Node *) update->value;

    this->Expression((const Node *) update->value);

    this->unit_->Emit(vm::OpCode::DUP, 1, nullptr, nullptr);
    this->unit_->IncrementStack(1);

    if (update->token_type == scanner::TokenType::PLUS_PLUS)
        this->unit_->Emit(vm::OpCode::INC, &update->loc);
    else if (update->token_type == scanner::TokenType::MINUS_MINUS)
        this->unit_->Emit(vm::OpCode::DEC, &update->loc);
    else
        throw CompilerException("");

    if (value->node_type == parser::NodeType::IDENTIFIER)
        this->StoreVariable((String *) ((const Unary *) value)->value);
    else if (value->node_type == parser::NodeType::INDEX) {
        // TODO
    } else if (value->node_type == parser::NodeType::SELECTOR) {
// TODO
    }
}

void Compiler::Expression(const Node *node) {
    switch (node->node_type) {
        case NodeType::ELVIS:
            this->CompileElvis((const Test *) node);
            break;
        case NodeType::LITERAL:
            this->LoadStatic(((const Unary *) node)->value, true, true);
            break;
        case NodeType::UNARY:
            this->CompileUnary((const Unary *) node);
            break;
        case NodeType::BINARY:
            if (node->token_type == scanner::TokenType::AND || node->token_type == scanner::TokenType::OR) {
                this->CompileTest((const parser::Binary *) node);
                break;
            }

            this->Binary((const parser::Binary *) node);
            break;
        case NodeType::TERNARY:
            this->CompileTernary((const parser::Test *) node);
            break;
        case NodeType::IDENTIFIER:
            this->LoadIdentifier((String *) ((const Unary *) node)->value);
            break;
        case NodeType::INIT:
            this->CompileInit((const parser::Initialization *) node);
            break;
        case NodeType::LIST:
        case NodeType::TUPLE:
        case NodeType::DICT:
        case NodeType::SET:
            this->CompileLTDS((const Unary *) node);
            break;
        case NodeType::UPDATE:
            this->CompileUpdate((const Unary *) node);
            break;
        case NodeType::SAFE_EXPR:
            this->CompileSafe((const Unary *) node);
            break;
        case NodeType::SELECTOR:
            this->CompileSelector((const parser::Binary *) node, false, true);
            break;
        case NodeType::INDEX:
        case NodeType::SLICE:
            this->CompileSubscr((const Subscript *) node, false, true);
            break;
    }
}

void Compiler::LoadIdentifier(String *identifier) {
    // N.B. Unknown variable, by default does not raise an error,
    // but tries to load it from the global namespace.
    SymbolT *sym;
    ArSize sym_id;

    if (StringEqual(identifier, (const char *) "_"))
        throw CompilerException("cannot use '_' as value");

    if ((sym = this->IdentifierLookupOrCreate(identifier, SymbolType::VARIABLE)) == nullptr)
        throw CompilerException("");

    sym_id = sym->id;

    Release(sym);

    auto scope = this->unit_->symt->type;
    if (scope != SymbolType::STRUCT &&
        scope != SymbolType::TRAIT &&
        sym->nested > 0) {
        if (sym->declared) {
            this->unit_->Emit(vm::OpCode::LDLC, (int) sym_id, nullptr, nullptr);
            return;
        } else if (sym->free) {
            this->unit_->Emit(vm::OpCode::LDENC, (int) sym_id, nullptr, nullptr);
            return;
        }
    }

    this->unit_->Emit(vm::OpCode::LDGBL, (int) sym_id, nullptr, nullptr);
}

void Compiler::StoreVariable(String *name) {
    vm::OpCode code = vm::OpCode::STGBL;
    SymbolT *sym;

    if (StringEqual(name, (const char *) "_"))
        this->unit_->Emit(vm::OpCode::POP, nullptr);

    if ((sym = this->IdentifierLookupOrCreate(name, SymbolType::VARIABLE)) == nullptr)
        throw CompilerException("");

    if (sym->declared && (this->unit_->symt->type == SymbolType::FUNC || sym->nested > 0))
        code = vm::OpCode::STLC;
    else if (sym->free)
        code = vm::OpCode::STENC;

    auto sym_id = sym->id;

    Release(sym);

    this->unit_->Emit(code, (int) sym_id, nullptr, nullptr);
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