// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

NODE_NEW(Unary, node::type_ast_identifier_, Identifier, nullptr, unary_dtor, nullptr);
NODE_NEW(Unary, node::type_ast_literal_, Literal, nullptr, unary_dtor, nullptr);
NODE_NEW(Unary, node::type_ast_prefix_, Prefix, nullptr, unary_dtor, nullptr);
NODE_NEW(Unary, node::type_ast_unary_, Unary, nullptr, unary_dtor, nullptr);
NODE_NEW(Unary, node::type_ast_update_, Update, nullptr, unary_dtor, nullptr);