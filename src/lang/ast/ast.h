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
        IDENTIFIER,
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
        TEST,
        LOGICAL,
        EQUALITY,
        RELATIONAL,
        NOT,
        BINARY_OP,
        UNARY_OP,
        MEMBER,
        MEMBER_SAFE,
        MEMBER_ASSERT,
        UPDATE,
        LIST,
        SET,
        MAP,
        SCOPE,
        LITERAL
    };

    struct Node {
        NodeType type;
        scanner::Pos start = 0;
        scanner::Pos end = 0;

        explicit Node(NodeType type, scanner::Pos start, scanner::Pos end) : type(type), start(start), end(end) {}

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

    struct Program : Node {
        std::list<NodeUptr> body;
        std::string filename;

        explicit Program(scanner::Pos start) : Node(NodeType::PROGRAM, start, 0) {}

        void AddStatement(NodeUptr statement) {
            this->body.push_back(std::move(statement));
        }

        void SetEndPos(scanner::Pos end) {
            this->end = end;
        }
    };

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

        explicit If(NodeUptr test, NodeUptr body, NodeUptr orelse, scanner::Pos start, scanner::Pos end) : If(
                std::move(test), std::move(body), start, end) {
            this->orelse = std::move(orelse);
        }

        explicit If(NodeUptr test, NodeUptr body, scanner::Pos start, scanner::Pos end) : Node(NodeType::IF, start,
                                                                                               end) {
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
        NodeUptr callee;
        std::list<NodeUptr> args;

        explicit Call(NodeUptr callee) : Node(NodeType::CALL, 0, 0) {
            this->callee = std::move(callee);
            this->start = this->callee->start;
        }

        void AddArgument(NodeUptr argument) {
            this->args.push_back(std::move(argument));
        }
    };

    struct Function : Node {
        std::string id;
        std::list<NodeUptr> params;
        NodeUptr body;
        bool pub;

        Function(std::string &id, std::list<NodeUptr> params, NodeUptr body, scanner::Pos start) : Node(NodeType::FUNC,
                                                                                                        start, 0) {
            this->id = id;
            this->params = std::move(params);
            this->body = std::move(body);
            this->end = this->body->end;
        }

        Function(std::list<NodeUptr> params, NodeUptr body, scanner::Pos start) : Node(NodeType::FUNC, start, 0) {
            this->params = std::move(params);
            this->body = std::move(body);
            this->end = this->body->end;
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

    struct Slice : Node {
        NodeUptr low;
        NodeUptr high;
        NodeUptr step;

        explicit Slice(NodeUptr low, NodeUptr high, NodeUptr step) : Node(NodeType::INDEX, 0, 0) {
            this->low = std::move(low);
            this->start = this->low->start;
            if (high != nullptr) {
                this->high = std::move(high);
                this->type = NodeType::SLICE;
                this->end = this->high->end;
                if (step != nullptr) {
                    this->step = std::move(step);
                    this->end = this->step->end;
                }
            }
        }
    };

    struct Binary : Node {
        NodeUptr left;
        NodeUptr right;
        lang::scanner::TokenType kind;

        explicit Binary(NodeType type, lang::scanner::TokenType kind, NodeUptr left, NodeUptr right) : Node(type, 0,
                                                                                                            0) {
            this->left = std::move(left);
            this->right = std::move(right);
            this->kind = kind;
            this->start = this->left->start;
            this->end = this->right->end;
        }

        explicit Binary(NodeType type, NodeUptr left, NodeUptr right) : Binary(type, scanner::TokenType::TK_NULL,
                                                                               std::move(left), std::move(right)) {}
    };

    struct Unary : Node {
        NodeUptr expr;
        lang::scanner::TokenType kind;

        explicit Unary(NodeType type, lang::scanner::TokenType kind, NodeUptr expr, scanner::Pos start,
                       scanner::Pos end) : Node(type, start, end) {
            this->expr = std::move(expr);
            this->kind = kind;
        }

        explicit Unary(NodeType type, NodeUptr expr, scanner::Pos end) : Unary(type, scanner::TokenType::TK_NULL,
                                                                               std::move(expr), 0, end) {
            this->start = this->expr->start;
        }
    };

    struct List : Node {
        std::list<NodeUptr> expressions;

        explicit List(NodeType type, scanner::Pos start) : Node(type, start, 0) {}

        void AddExpression(NodeUptr expr) {
            this->end = expr->end;
            this->expressions.push_back(std::move(expr));
        }
    };

    struct Update : Node {
        NodeUptr expr;
        scanner::TokenType kind;
        bool prefix;

        explicit Update(NodeUptr expr, scanner::TokenType kind, bool prefix, scanner::Pos start, scanner::Pos end)
                : Node(NodeType::UPDATE, start, end) {
            this->expr = std::move(expr);
            this->kind = kind;
            this->prefix = prefix;
        }

        explicit Update(NodeUptr expr, scanner::TokenType kind, bool prefix, scanner::Pos end) : Update(std::move(expr),
                                                                                                        kind, prefix, 0,
                                                                                                        end) {
            this->start = this->expr->start;
        }
    };

    struct Literal : Node {
        scanner::TokenType kind;
        std::string value;

        explicit Literal(const scanner::Token &token) : Node(NodeType::LITERAL, token.start, token.end) {
            this->kind = token.type;
            this->value = token.value;
        }
    };

    struct Scope : Node {
        std::list<std::string> segments;

        explicit Scope(scanner::Pos start) : Node(NodeType::SCOPE, start, 0) {}

        void AddSegment(const std::string &segment) {
            this->segments.push_back(segment);
        }
    };

    struct Identifier : Node {
        std::string value;
        bool rest_element = false;

        explicit Identifier(const scanner::Token &token) : Node(NodeType::IDENTIFIER, token.start, token.end) {
            this->value = token.value;
        }
    };

} // namespace lang::ast

#endif // !ARGON_LANG_AST_AST_H_
