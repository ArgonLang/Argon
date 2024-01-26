// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

bool assignment_dtor(Assignment *self) {
    Release(self->name);
    Release(self->value);

    return true;
}

NODE_NEW(Assignment, node::type_ast_assignment_, Assignment, nullptr, assignment_dtor, nullptr);