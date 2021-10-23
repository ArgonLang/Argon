// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER_NODES_H_
#define ARGON_LANG_PARSER_NODES_H_

#include <lang/scanner/token2.h>

#include <object/arobject.h>
#include <object/datatype/list.h>
#include <object/datatype/string.h>

namespace argon::lang::parser {
    struct Node : object::ArObject {
        scanner2::TokenType kind;

        argon::lang::scanner2::Pos start;
        argon::lang::scanner2::Pos end;

        argon::lang::scanner2::Pos colno;
        argon::lang::scanner2::Pos lineno;
    };

    // extern const object::TypeInfo *type_ast_node_;

    struct Assignment : Node {
        bool pub;
        bool weak;

        object::ArObject *name;
        object::ArObject *value;
    };
    extern const object::TypeInfo *type_ast_let_;
    extern const object::TypeInfo *type_ast_var_;

    struct Unary : Node {
        object::ArObject *value;
    };
    extern const object::TypeInfo *type_ast_unary_;
    extern const object::TypeInfo *type_ast_literal_;
    extern const object::TypeInfo *type_ast_identifier_;
    extern const object::TypeInfo *type_ast_scope_;
    extern const object::TypeInfo *type_ast_restid_;
    extern const object::TypeInfo *type_ast_spread_;
    extern const object::TypeInfo *type_ast_list_;
    extern const object::TypeInfo *type_ast_tuple_;
    extern const object::TypeInfo *type_ast_map_;
    extern const object::TypeInfo *type_ast_set_;
    extern const object::TypeInfo *type_ast_expression_;
    extern const object::TypeInfo *type_ast_list_decl_;
    extern const object::TypeInfo *type_ast_block_;
    extern const object::TypeInfo *type_ast_ret_;
    extern const object::TypeInfo *type_ast_jmp_;
    extern const object::TypeInfo *type_ast_safe_;

    struct Binary : Node {
        object::ArObject *left;
        object::ArObject *right;
    };
    extern const object::TypeInfo *type_ast_binary_;
    extern const object::TypeInfo *type_ast_selector_;
    extern const object::TypeInfo *type_ast_init_;
    extern const object::TypeInfo *type_ast_kwinit_;
    extern const object::TypeInfo *type_ast_assignment_;
    extern const object::TypeInfo *type_ast_call_;
    extern const object::TypeInfo *type_ast_import_name_;
    extern const object::TypeInfo *type_ast_switch_case_;
    extern const object::TypeInfo *type_ast_label_;

    struct UpdateIncDec : Node {
        object::ArObject *value;
        bool prefix;
    };
    extern const object::TypeInfo *type_ast_update_;

    struct Subscript : Node {
        object::ArObject *left;

        object::ArObject *low;
        object::ArObject *high;
        object::ArObject *step;
    };
    extern const object::TypeInfo *type_ast_index_;
    extern const object::TypeInfo *type_ast_subscript_;

    struct Test : Node {
        object::ArObject *test;
        object::ArObject *body;
        object::ArObject *orelse;
    };
    extern const object::TypeInfo *type_ast_elvis_;
    extern const object::TypeInfo *type_ast_test_;
    extern const object::TypeInfo *type_ast_switch_;

    struct Loop : Node {
        Node *init;
        Node *test;
        Node *inc;
        Node *body;
    };
    extern const object::TypeInfo *type_ast_loop_;
    extern const object::TypeInfo *type_ast_for_;
    extern const object::TypeInfo *type_ast_for_in_;

    struct File : Node {
        object::String *name;
        object::List *decls;
    };
    extern const object::TypeInfo *type_ast_file_;

    struct Construct : Node {
        object::String *name;
        object::ArObject *params;
        Node *block;

        bool pub;
    };
    extern const object::TypeInfo *type_ast_func_;
    extern const object::TypeInfo *type_ast_trait_;
    extern const object::TypeInfo *type_ast_struct_;

    struct ImportDecl : Node {
        object::String *module;
        object::ArObject *names;
        bool star;
    };
    extern const object::TypeInfo *type_ast_import_decl_;

    Unary *UnaryNew(scanner2::TokenType kind, scanner2::Pos start, Node *right);

    Unary *SpreadNew(Node *left, scanner2::Pos end);

    UpdateIncDec *UpdateNew(scanner2::TokenType kind, scanner2::Pos start_end, bool prefix, Node *value);

    Binary *BinaryNew(scanner2::TokenType kind, const argon::object::TypeInfo *type, Node *left, Node *right);

    Binary *InitNew(Node *left, object::ArObject *args, scanner2::Pos end, bool kwinit);

    Subscript *SubscriptNew(object::ArObject *left, bool slice);

    Assignment *AssignmentNew(scanner2::Token &token, bool constant, bool pub, bool weak);

    Construct *
    FunctionNew(scanner2::Pos start, argon::object::String *name, object::List *params, Node *block, bool pub);

    ImportDecl *ImportNew(object::String *module, object::ArObject *names, scanner2::Pos start);
}

#endif // !ARGON_LANG_PARSER_NODES_H_
