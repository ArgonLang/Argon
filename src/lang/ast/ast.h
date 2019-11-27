// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_AST_AST_H_
#define ARGON_LANG_AST_AST_H_

#include <list>

#include <lang/scanner/token.h>

namespace lang::ast {
    enum class NodeType {
        TUPLE,
        ELVIS,
        OR_TEST,
        AND_TEST,
        LOGICAL_OR,
        LOGICAL_XOR,
        LOGICAL_AND,
        EQUALITY,
        RELATIONAL,
        SHL,
        SHR,
        SUM,
        SUB,
        MUL,
        DIV,
        INTEGER_DIV,
        REMINDER,
        NOT,
        BITWISE_NOT,
        PLUS,
        MINUS,
        PREFIX_INC,
        PREFIX_DEC,
        MEMBER,
        MEMBER_SAFE,
        MEMBER_ASSERT,
        POSTFIX_INC,
        POSTFIX_DEC,
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

    struct List : Node {
        std::list<NodeUptr> expressions;

        explicit List(NodeType type, unsigned colno, unsigned lineno) : Node(type, colno, lineno) {}

        void AddExpression(NodeUptr expr) {
            this->expressions.push_front(std::move(expr));
        }
    };

    struct Binary : Node {
        NodeUptr left;
        NodeUptr right;
        lang::scanner::TokenType kind;

        explicit Binary(NodeType type, lang::scanner::TokenType kind, NodeUptr left, NodeUptr right, unsigned colno,
                        unsigned lineno) : Node(type, colno, lineno) {
            this->left = std::move(left);
            this->right = std::move(right);
            this->kind = kind;
        }
    };

    struct Unary : Node {
        NodeUptr expr;

        explicit Unary(NodeType type, NodeUptr expr, unsigned colno, unsigned lineno) : Node(type, colno, lineno) {
            this->expr = std::move(expr);
        }
    };

    struct Scope : Node {
        std::list<std::string> segments;

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
