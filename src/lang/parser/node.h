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
        AWAIT,
        BINARY,
        BLOCK,
        CALL,
        DECLARATION,
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
        NULL_COALESCING,
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
    extern const vm::datatype::TypeInfo *type_ast_assignment_;

    struct Binary {
        NODEOBJ_HEAD;

        Node *left;
        Node *right;
    };
    extern const vm::datatype::TypeInfo *type_ast_binary_;

    struct Call {
        NODEOBJ_HEAD;

        Node *left;
        argon::vm::datatype::ArObject *args;
        argon::vm::datatype::ArObject *kwargs;
    };
    extern const vm::datatype::TypeInfo *type_ast_call_;

    struct Construct {
        NODEOBJ_HEAD;

        bool pub;

        vm::datatype::String *name;
        vm::datatype::String *doc;

        vm::datatype::List *impls;
        Node *body;
    };
    extern const vm::datatype::TypeInfo *type_ast_construct_;

    struct File {
        NODEOBJ_HEAD;

        char *filename;

        vm::datatype::String *doc;

        vm::datatype::List *statements;
    };
    extern const vm::datatype::TypeInfo *type_ast_file_;

    struct Function {
        NODEOBJ_HEAD;

        bool async;
        bool pub;

        vm::datatype::String *name;
        vm::datatype::String *doc;

        vm::datatype::List *params;
        Node *body;
    };
    extern const vm::datatype::TypeInfo *type_ast_function_;

    struct Import {
        NODEOBJ_HEAD;

        bool pub;

        Node *mod;
        argon::vm::datatype::ArObject *names;
    };
    extern const vm::datatype::TypeInfo *type_ast_import_;

    struct Initialization {
        NODEOBJ_HEAD;

        bool as_map;

        Node *left;
        argon::vm::datatype::ArObject *values;
    };
    extern const vm::datatype::TypeInfo *type_ast_initialization_;

    struct Loop {
        NODEOBJ_HEAD;

        Node *init;
        Node *test;
        Node *inc;
        Node *body;
    };
    extern const vm::datatype::TypeInfo *type_ast_loop_;

    struct Subscript {
        NODEOBJ_HEAD;

        Node *expression;
        Node *start;
        Node *stop;
    };
    extern const vm::datatype::TypeInfo *type_ast_subscript_;

    struct SwitchCase {
        NODEOBJ_HEAD;

        vm::datatype::ArObject *conditions;
        vm::datatype::ArObject *body;
    };
    extern const vm::datatype::TypeInfo *type_ast_switchcase_;

    struct Test {
        NODEOBJ_HEAD;

        Node *test;
        Node *body;
        Node *orelse;
    };
    extern const vm::datatype::TypeInfo *type_ast_test_;

    struct Unary {
        NODEOBJ_HEAD;

        argon::vm::datatype::ArObject *value;
    };
    extern const vm::datatype::TypeInfo *type_ast_unary_;

    Assignment *AssignmentNew(vm::datatype::ArObject *name, bool constant, bool pub, bool weak);

    Binary *BinaryNew(Node *left, Node *right, scanner::TokenType token, NodeType type);

    Call *CallNew(Node *left, vm::datatype::ArObject *args, vm::datatype::ArObject *kwargs);

    Construct *ConstructNew(vm::datatype::String *name, vm::datatype::List *impls, Node *body, NodeType type, bool pub);

    File *FileNew(const char *filename, vm::datatype::List *statements);

    Function *FunctionNew(vm::datatype::String *name, vm::datatype::List *params, Node *body, bool pub);

    Import *ImportNew(Node *mod, argon::vm::datatype::ArObject *names, bool pub);

    Initialization *InitNew(Node *left, vm::datatype::ArObject *list, const scanner::Loc &loc, bool as_map);

    Loop *LoopNew(Node *init, Node *test, Node *inc, Node *body, NodeType type);

    Subscript *SubscriptNew(Node *expr, Node *start, Node *stop, bool slice);

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

