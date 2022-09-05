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
        ASSERT,
        ASSIGNMENT,
        BINARY,
        BLOCK,
        CALL,
        DICT,
        ELVIS,
        ELLIPSIS,
        EXPRESSION,
        FILE,
        FOR,
        FOREACH,
        FUNC,
        IDENTIFIER,
        IF,
        IMPORT,
        IMPORT_NAME,
        INDEX,
        IN,
        INIT,
        JUMP,
        LABEL,
        LIST,
        LITERAL,
        LOOP,
        KWARG,
        PANIC,
        REST,
        RETURN,
        SAFE_EXPR,
        SELECTOR,
        SET,
        SLICE,
        STRUCT,
        SWITCH,
        SWITCH_CASE,
        TERNARY,
        TRAIT,
        TRAP,
        TUPLE,
        UNARY,
        UPDATE,
        YIELD
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
        vm::datatype::String *doc;

        vm::datatype::List *impls;
        Node *body;
    };

    struct File {
        NODEOBJ_HEAD;

        const char *filename;

        vm::datatype::String *doc;

        vm::datatype::List *statements;
    };

    struct Function {
        NODEOBJ_HEAD;

        bool async;
        bool pub;

        vm::datatype::String *name;
        vm::datatype::String *doc;

        vm::datatype::List *params;
        Node *body;
    };

    struct Import {
        NODEOBJ_HEAD;

        Node *module;
        argon::vm::datatype::ArObject *names;
    };

    struct Initialization {
        NODEOBJ_HEAD;

        bool as_map;

        Node *left;
        argon::vm::datatype::ArObject *values;
    };

    struct Loop {
        NODEOBJ_HEAD;

        Node *init;
        Node *test;
        Node *inc;
        Node *body;
    };

    struct Subscript {
        NODEOBJ_HEAD;

        Node *expression;
        Node *start;
        Node *stop;
    };

    struct SwitchCase {
        NODEOBJ_HEAD;

        vm::datatype::ArObject *conditions;
        vm::datatype::ArObject *body;
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

    Function *FunctionNew(vm::datatype::String *name, vm::datatype::List *params, Node *body, bool pub);

    Import *ImportNew(Node *module, argon::vm::datatype::ArObject *names);

    Initialization *InitNew(Node *left, vm::datatype::ArObject *list, const scanner::Loc &loc, bool as_map);

    Loop *LoopNew(Node *init, Node *test, Node *inc, Node *body, NodeType type);

    Subscript *SubscriptNew(Node *expr, Node *start, Node *stop);

    SwitchCase *
    SwitchCaseNew(vm::datatype::ArObject *conditions, vm::datatype::ArObject *body, const scanner::Loc &loc);

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

