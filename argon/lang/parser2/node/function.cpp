// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

bool function_dtor(Function *self) {
    Release(self->name);
    Release(self->doc);

    Release(self->params);
    Release(self->body);

    return true;
}

NODE_NEW(Function, node::type_ast_function_, Function, nullptr, function_dtor, nullptr);