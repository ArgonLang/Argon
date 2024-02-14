// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

bool loop_dtor(Loop *self) {
    Release(self->init);
    Release(self->test);
    Release(self->inc);
    Release(self->body);

    return true;
}

NODE_NEW(Loop, node::type_ast_loop_, Loop, nullptr, loop_dtor, nullptr);