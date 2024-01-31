// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

NODE_NEW(Binary, node::type_ast_sync_, Sync, nullptr, binary_dtor, nullptr);
