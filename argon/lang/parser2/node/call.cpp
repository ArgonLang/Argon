// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

bool call_dtor(Call *self) {
    Release(self->left);
    Release(self->args);
    Release(self->kwargs);

    return true;
}

NODE_NEW(Call, node::type_ast_call_, Call, nullptr, call_dtor, nullptr);