// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

bool subscript_dtor(Subscript *self) {
    Release(self->expression);
    Release(self->start);
    Release(self->stop);

    return true;
}

NODE_NEW(Subscript, node::type_ast_subscript_, Subscript, nullptr, subscript_dtor, nullptr);