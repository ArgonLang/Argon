// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/compiler2/compiler2.h>

using namespace argon::vm::datatype;
using namespace argon::lang::compiler2;
using namespace argon::lang::parser2;

void Compiler::Compile(const node::Node *node) {
    switch (node->node_type) {
        case node::NodeType::EXPRESSION:
            break;
        default:
            assert(false);
    }
}

void Compiler::EnterScope(String *name, SymbolType type) {
    auto *unit = TranslationUnitNew(this->unit_, name, type);
    this->unit_ = unit;
}

void Compiler::ExitScope() {
    this->unit_ = TranslationUnitDel(this->unit_);
}

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
