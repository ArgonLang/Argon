// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

bool objectinit_dtor(ObjectInit *self) {
    Release(self->left);
    Release(self->values);

    return true;
}

NODE_NEW(ObjectInit, node::type_ast_objinit_, ObjectInit, nullptr, objectinit_dtor, nullptr);