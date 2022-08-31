// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER_NODE_H_
#define ARGON_LANG_PARSER_NODE_H_

#include <lang/scanner/token.h>

#include <vm/datatype/arobject.h>
#include <vm/datatype/list.h>
#include <vm/datatype/arstring.h>

namespace argon::lang::parser {
#define NODEOBJ_HEAD                            \
    AROBJ_HEAD;                                 \
    const NodeType node_type;                   \
    argon::lang::scanner::TokenType token_type; \
    argon::lang::scanner::Loc loc

    enum class NodeType {
        ASSIGNMENT,
        BINARY,
        BLOCK,
        CALL,
        DICT,
        ELVIS,
        ELLIPSIS,
        EXPRESSION,
        IDENTIFIER,
        INDEX,
        INIT,
        LIST,
        LITERAL,
        KWARG,
        FILE,
        FUNC,
        REST,
        SAFE_EXPR,
        SELECTOR,
        SET,
        SLICE,
        STRUCT,
        TERNARY,
        TRAIT,
        TUPLE,
        UNARY,
        UPDATE
    };

    struct Node {
        NODEOBJ_HEAD;
    };

    struct Assignment {
        NODEOBJ_HEAD;

        bool constant;
        bool multi;
        bool pub;
        bool weak;

        argon::vm::datatype::ArObject *name;
        argon::vm::datatype::ArObject *value;
    };

    struct Binary {
        NODEOBJ_HEAD;

        Node *left;
        Node *right;
    };

    struct Call {
        NODEOBJ_HEAD;

        Node *left;
        argon::vm::datatype::ArObject *args;
        argon::vm::datatype::ArObject *kwargs;
    };

    struct Construct {
        NODEOBJ_HEAD;

        vm::datatype::String *name;
        vm::datatype::List *impls;
        Node *body;
    };

    struct File {
        NODEOBJ_HEAD;

        const char *filename;

        vm::datatype::List *statements;
    };

    struct Function {
        NODEOBJ_HEAD;

        vm::datatype::String *name;
        vm::datatype::List *params;
        Node *body;
    };

    struct Initialization {
        NODEOBJ_HEAD;

        bool as_map;

        Node *left;
        argon::vm::datatype::ArObject *values;
    };

    struct Subscript {
        NODEOBJ_HEAD;

        Node *expression;
        Node *start;
        Node *stop;
    };

    struct Test {
        NODEOBJ_HEAD;

        Node *test;
        Node *body;
        Node *orelse;
    };

    struct Unary {
        NODEOBJ_HEAD;

        argon::vm::datatype::ArObject *value;
    };

    Assignment *AssignmentNew(vm::datatype::ArObject *name, bool constant, bool pub, bool weak);

    Binary *BinaryNew(Node *left, Node *right, scanner::TokenType token, NodeType type);

    Call *CallNew(Node *left, vm::datatype::ArObject *args, vm::datatype::ArObject *kwargs);

    Construct *ConstructNew(vm::datatype::String *name, vm::datatype::List *impls, Node *body, NodeType type);

    File *FileNew(const char *filename, vm::datatype::List *statements);

    Function *FunctionNew(vm::datatype::String *name, vm::datatype::List *params, Node *body);

    Initialization *InitNew(Node *left, vm::datatype::ArObject *list, const scanner::Loc &loc, bool as_map);

    Subscript *SubscriptNew(Node *expr, Node *start, Node *stop);

    Test *TestNew(Node *test, Node *body, Node *orelse, NodeType type);

    Unary *UnaryNew(argon::vm::datatype::ArObject *value, NodeType type, const scanner::Loc &loc);

    inline Unary *UnaryNew(argon::vm::datatype::ArObject *value, scanner::TokenType type, const scanner::Loc &loc) {
        auto *unary = UnaryNew(value, NodeType::UNARY, loc);
        if (unary != nullptr)
            unary->token_type = type;

        return unary;
    }

    template<typename T>
    T *NodeNew(const argon::vm::datatype::TypeInfo *type, NodeType kind) {
        auto *node = argon::vm::datatype::MakeObject<T>(type);
        if (node == nullptr)
            return nullptr;

        *((NodeType *) &(node->node_type)) = kind;
        node->loc = scanner::Loc{};

        return node;
    }

} // namespace argon::lang::parser

#endif // !ARGON_LANG_PARSER_NODE_H_

