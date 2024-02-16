// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/parser2/node/node.h>

using namespace argon::lang::parser2::node;

NODE_NEW(Binary, node::type_ast_assertion_, Assertion, nullptr, binary_dtor, nullptr);
NODE_NEW(Binary, node::type_ast_import_name_, ImportName, nullptr, binary_dtor, nullptr);
NODE_NEW(Binary, node::type_ast_infix_, Infix, nullptr, binary_dtor, nullptr);
NODE_NEW(Binary, node::type_ast_selector_, Selector, nullptr, binary_dtor, nullptr);
NODE_NEW(Binary, node::type_ast_switchcase_, SwitchCase, nullptr, binary_dtor, nullptr);
NODE_NEW(Binary, node::type_ast_sync_, Sync, nullptr, binary_dtor, nullptr);
NODE_NEW(Binary, node::type_ast_binary_, Binary, nullptr, binary_dtor, nullptr);
