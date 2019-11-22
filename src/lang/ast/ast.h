// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_AST_AST_H_
#define ARGON_LANG_AST_AST_H_

#include <list>

#include <lang/scanner/token.h>

namespace lang::ast {
    enum class NodeType {
        SCOPE,
        LITERAL,
        SYNTAX_ERROR
    };

    struct Node {
        NodeType type;
        unsigned colno = 0;
        unsigned lineno = 0;

        explicit Node(NodeType type, unsigned colno, unsigned lineno) : type(type), colno(colno), lineno(lineno) {}

        virtual ~Node() = default;

        virtual std::string String() { return ""; }
    };

    using NodeUptr = std::unique_ptr<const Node>;

    template<typename T>
    constexpr T *CastNode(const NodeUptr &node) {
        return ((T *) node.get());
    }

    // **********************************************
    // NODES
    // **********************************************

    struct Scope : Node {
        std::list<const std::string> segments;

        explicit Scope(const lang::scanner::Token &token) : Node(NodeType::SCOPE, token.colno, token.lineno) {}

        void AddSegment(const std::string &segment) {
            this->segments.push_front(segment);
        }
    };

    struct Literal : Node {
        lang::scanner::TokenType kind;
        std::string value;

        explicit Literal(const lang::scanner::Token &token) : Node(NodeType::LITERAL, token.colno, token.lineno) {
            this->kind = token.type;
            this->value = token.value;
        }
    };

    struct SyntaxError : Node {
        std::string what;

        explicit SyntaxError(const std::string &what, unsigned colno, unsigned lineno) : Node(NodeType::SYNTAX_ERROR,
                                                                                              colno, lineno) {
            this->what = what;
        }
    };

} // namespace lang::ast

#endif // !ARGON_LANG_AST_AST_H_
