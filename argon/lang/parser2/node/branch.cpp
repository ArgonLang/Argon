// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

bool branch_dtor(Branch *self) {
    Release(self->test);
    Release(self->body);
    Release(self->orelse);

    return true;
}

NODE_NEW(Branch, node::type_ast_branch_, Branch, nullptr, branch_dtor, nullptr);