// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

bool module_dtor(Module *self) {
    Release(self->filename);
    Release(self->docs);

    Release(self->statements);

    return true;
}

NODE_NEW(Module, node::type_ast_module_, Module, nullptr, module_dtor, nullptr);