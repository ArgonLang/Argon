// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER_NODE_H_
#define ARGON_LANG_PARSER_NODE_H_

#include <lang/scanner/token.h>

#include <vm/datatype/list.h>
#include <vm/datatype/arobject.h>

namespace argon::lang::parser {
#define NODEOBJ_HEAD                            \
    AROBJ_HEAD;                                 \
    const NodeType node_type;                   \
    argon::lang::scanner::TokenType token_type; \
    argon::lang::scanner::Loc loc

    enum class NodeType {
        BINARY,
        DICT,
        IDENTIFIER,
        LIST,
        LITERAL,
        FILE,
        SET,
        UNARY
    };

    struct Node {
        NODEOBJ_HEAD;
    };

    struct File {
        NODEOBJ_HEAD;

        const char *filename;

        vm::datatype::List *statements;
    };

    struct Binary {
        NODEOBJ_HEAD;

        Node *left;
        Node *right;
    };

    struct Unary {
        NODEOBJ_HEAD;

        argon::vm::datatype::ArObject *value;
    };

    File *FileNew(const char *filename, vm::datatype::List *statements);

    Binary *BinaryNew(Node *left, Node *right, scanner::TokenType token, NodeType type);

    Unary *UnaryNew(argon::vm::datatype::ArObject *value, NodeType type, const scanner::Loc &loc);

    inline Unary *UnaryNew(argon::vm::datatype::ArObject *value, scanner::TokenType type, const scanner::Loc &loc) {
        auto *unary = UnaryNew(value, NodeType::UNARY, loc);
        if(unary!= nullptr)
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

