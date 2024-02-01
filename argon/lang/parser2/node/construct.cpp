// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

bool construct_dtor(Construct *self) {
    Release(self->name);
    Release(self->doc);
    Release(self->impls);

    Release(self->body);

    return true;
}

NODE_NEW(Construct, node::type_ast_struct_, Struct, nullptr, construct_dtor, nullptr);
NODE_NEW(Construct, node::type_ast_trait_, Trait, nullptr, construct_dtor, nullptr);