// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

bool import_dtor(Import *self){
    Release(self->mod);
    Release(self->names);

    return true;
}

NODE_NEW(Import, node::type_ast_import_, Import, nullptr, import_dtor, nullptr);
