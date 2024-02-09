// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

bool parameter_dtor(Assignment *self) {
    Release(self->name);
    Release(self->value);

    return true;
}

NODE_NEW(Parameter, node::type_ast_argument_, Argument, nullptr, parameter_dtor, nullptr);
NODE_NEW(Parameter, node::type_ast_parameter_, Parameter, nullptr, parameter_dtor, nullptr);