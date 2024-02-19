// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/compiler2/compiler2.h>

using namespace argon::vm::datatype;
using namespace argon::lang::compiler2;
using namespace argon::lang::parser2;

#define CHECK_AST_NODE(expected, chk)                                                       \
do {                                                                                        \
    if(!AR_TYPEOF(chk, expected))                                                           \
        throw CompilerException(kCompilerErrors[0], (expected)->name, AR_TYPE_NAME(chk));   \
} while(0)

int Compiler::LoadStatic(const node::Unary *literal, bool store, bool emit) {
    CHECK_AST_NODE(node::type_ast_literal_, literal);

    auto *value = IncRef(literal->value);
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
        this->unit_->Emit(vm::OpCode::LSTATIC, idx, nullptr, &literal->loc);

    return idx;
}

void Compiler::Compile(const node::Node *node) {
    switch (node->node_type) {
        case node::NodeType::EXPRESSION:
            break;
        default:
            assert(false);
    }
}

// *********************************************************************************************************************
// EXPRESSION-ZONE
// *********************************************************************************************************************

void Compiler::Expression(const node::Node *node) {
    switch (node->node_type) {
        case node::NodeType::AWAIT:
            break;
        case node::NodeType::LITERAL:
            this->LoadStatic((const node::Unary *) node, true, true);
            break;
        default:
            assert(false);
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
    } catch (...) {
        assert(false);
    }

    return nullptr;
}
