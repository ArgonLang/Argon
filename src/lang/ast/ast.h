// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_AST_AST_H_
#define ARGON_LANG_AST_AST_H_

#include <list>

#include <lang/scanner/token.h>

namespace lang::ast {
    enum class NodeType {
        PROGRAM,
        VARIADIC,
        ELLIPSIS,
        ALIAS,
        SUBSCRIPT,
        INDEX,
        SLICE,
        CALL,
        FOR,
        FOR_IN,
        FALLTHROUGH,
        BREAK,
        CONTINUE,
        GOTO,
        LOOP,
        SWITCH,
        CASE,
        IF,
        IMPORT,
        IMPORT_ALIAS,
        VARIABLE,
        CONSTANT,
        FUNC,
        RETURN,
        DEFER,
        SPAWN,
        STRUCT,
        STRUCT_BLOCK,
        TRAIT,
        TRAIT_BLOCK,
        TRAIT_LIST,
        IMPL,
        BLOCK,
        ASSIGN,
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
        LIST,
        SET,
        MAP,
        SCOPE,
        LITERAL
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

    struct Block : Node {
        std::list<NodeUptr> stmts;

        Block(NodeType type, unsigned colno, unsigned lineno) : Node(type, colno, lineno) {}

        void AddStmtOrExpr(NodeUptr stmt) {
            this->stmts.push_front(std::move(stmt));
        }
    };

    struct For : Node {
        NodeUptr init;
        NodeUptr test;
        NodeUptr inc;
        NodeUptr body;

        explicit For(NodeUptr iter, NodeUptr iterexpr, NodeUptr body, unsigned colno, unsigned lineno) : Node(
                NodeType::FOR_IN, colno, lineno) {
            this->init = std::move(iter);
            this->test = std::move(iterexpr);
            this->inc = nullptr;
            this->body = std::move(body);
        }

        explicit For(NodeUptr inti, NodeUptr test, NodeUptr inc, NodeUptr body, unsigned colno, unsigned lineno) : Node(
                NodeType::FOR, colno, lineno) {
            this->init = std::move(init);
            this->test = std::move(test);
            this->inc = std::move(inc);
            this->body = std::move(body);
        }
    };

    struct Loop : Node {
        NodeUptr test;
        NodeUptr body;

        explicit Loop(NodeUptr test, NodeUptr body, unsigned colno, unsigned lineno) : Node(NodeType::LOOP, colno,
                                                                                            lineno) {
            this->test = std::move(test);
            this->body = std::move(body);
        }
    };

    struct Switch : Node {
        NodeUptr test;
        std::list<NodeUptr> cases;

        explicit Switch(NodeUptr test, unsigned colno, unsigned lineno) : Node(NodeType::SWITCH, colno, lineno) {
            this->test = std::move(test);
        }

        void AddCase(NodeUptr swcase) {
            this->cases.push_front(std::move(swcase));
        }
    };

    struct Case : Node {
        std::list<NodeUptr> tests;
        NodeUptr body;

        explicit Case(unsigned colno, unsigned lineno) : Node(NodeType::CASE, colno, lineno) {}

        void AddCondition(NodeUptr condition) {
            this->tests.push_front(std::move(condition));
        }
    };

    struct If : Node {
        NodeUptr test;
        NodeUptr body;
        NodeUptr orelse;

        explicit If(NodeUptr test, NodeUptr body, NodeUptr orelse, unsigned colno, unsigned lineno) : If(
                std::move(test), std::move(body), colno, lineno) {
            this->orelse = std::move(orelse);
        }

        explicit If(NodeUptr test, NodeUptr body, unsigned colno, unsigned lineno) : Node(NodeType::IF, colno, lineno) {
            this->test = std::move(test);
            this->body = std::move(body);
        }
    };

    struct Import : Node {
        std::string module;
        std::list<NodeUptr> names;

        explicit Import(unsigned colno, unsigned lineno) : Node(NodeType::IMPORT, colno, lineno) {}

        void AddName(NodeUptr name) {
            this->names.push_front(std::move(name));
        }
    };

    struct ImportAlias : Node {
        std::string path;
        std::string alias;

        explicit ImportAlias(std::string &path, std::string &alias, unsigned colno, unsigned lineno) : Node(
                NodeType::IMPORT_ALIAS, colno, lineno) {
            this->path = path;
            this->alias = alias;
        }
    };

    struct Call : Node {
        NodeUptr func;
        std::list<NodeUptr> args;

        Call(NodeUptr func, unsigned colno, unsigned lineno) : Node(NodeType::CALL, colno, lineno) {
            this->func = std::move(func);
        }

        void AddArgument(NodeUptr argument) {
            this->args.push_back(std::move(argument));
        }
    };

    struct Function : Node {
        std::string name;
        std::list<NodeUptr> params;
        NodeUptr body;
        bool pub;

        Function(std::string &name, std::list<NodeUptr> params, NodeUptr body, bool pub, unsigned colno,
                 unsigned lineno) : Node(NodeType::FUNC, colno, lineno) {
            this->name = name;
            this->params = std::move(params);
            this->body = std::move(body);
            this->pub = pub;
        }

        Function(std::list<NodeUptr> params, NodeUptr body, bool pub, unsigned colno, unsigned lineno) : Node(
                NodeType::FUNC, colno, lineno) {
            this->name = "";
            this->params = std::move(params);
            this->body = std::move(body);
            this->pub = pub;
        }
    };

    struct Construct : Node {
        std::string name;
        NodeUptr auxiliary;
        NodeUptr body;
        bool pub = false;

        explicit Construct(NodeType type, std::string &name, NodeUptr auxiliary, NodeUptr body, unsigned colno,
                           unsigned lineno)
                : Construct(type, name, colno, lineno) {
            this->auxiliary = std::move(auxiliary);
            this->body = std::move(body);
        }

        explicit Construct(NodeType type, std::string &name, unsigned colno, unsigned lineno) : Node(type,
                                                                                                     colno,
                                                                                                     lineno) {
            this->name = name;
        }
    };

    struct Variable : Node {
        bool atomic = false;
        bool weak = false;
        bool pub = false;
        std::string name;
        NodeUptr value;
        NodeUptr annotation;

        explicit Variable(std::string &name, NodeUptr value, bool constant, unsigned colno, unsigned lineno) : Node(
                NodeType::VARIABLE, colno, lineno) {
            if (constant)
                this->type = NodeType::CONSTANT;
            this->name = name;
            this->value = std::move(value);
        }
    };

    struct Impl : Node {
        NodeUptr name;
        NodeUptr target;
        NodeUptr block;

        explicit Impl(NodeUptr name, NodeUptr target, NodeUptr block, unsigned colno, unsigned lineno) : Node(
                NodeType::IMPL, colno, lineno) {
            this->name = std::move(name);
            this->target = std::move(target);
            this->block = std::move(block);
        }
    };

    struct List : Node {
        std::list<NodeUptr> expressions;

        explicit List(NodeType type, unsigned colno, unsigned lineno) : Node(type, colno, lineno) {}

        void AddExpression(NodeUptr expr) {
            this->expressions.push_front(std::move(expr));
        }
    };

    struct Slice : Node {
        NodeUptr low;
        NodeUptr high;
        NodeUptr step;

        explicit Slice(NodeUptr low, NodeUptr high, NodeUptr step, unsigned colno, unsigned lineno) : Node(
                NodeType::INDEX, colno, lineno) {
            this->low = std::move(low);
            if (high != nullptr) {
                this->high = std::move(high);
                this->type = NodeType::SLICE;
                if (step != nullptr)
                    this->step = std::move(step);
            }
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

        explicit Binary(NodeType type, NodeUptr left, NodeUptr right, unsigned colno,
                        unsigned lineno) : Binary(type, scanner::TokenType::TK_NULL, std::move(left), std::move(right),
                                                  colno, lineno) {}
    };

    struct Unary : Node {
        NodeUptr expr;

        explicit Unary(NodeType type, NodeUptr expr, unsigned colno, unsigned lineno) : Node(type, colno, lineno) {
            this->expr = std::move(expr);
        }
    };

    struct Scope : Node {
        std::list<std::string> segments;

        explicit Scope(unsigned colno, unsigned lineno) : Node(NodeType::SCOPE, colno, lineno) {}

        void AddSegment(const std::string &segment) {
            this->segments.push_front(segment);
        }
    };

    struct Literal : Node {
        lang::scanner::TokenType kind;
        std::string value;

        explicit Literal(const lang::scanner::Token &token) : Node(NodeType::LITERAL, token.start, token.end) {
            this->kind = token.type;
            this->value = token.value;
        }
    };
} // namespace lang::ast

#endif // !ARGON_LANG_AST_AST_H_
