// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_AST_AST_H_
#define ARGON_LANG_AST_AST_H_

#include <list>

#include <lang/scanner/token.h>

namespace lang::ast {
    enum class NodeType {
        ALIAS,
        ASSIGN,
        BINARY_OP,
        BLOCK,
        BREAK,
        CALL,
        CASE,
        COMMENT,
        CONSTANT,
        CONTINUE,
        DEFER,
        ELLIPSIS,
        ELVIS,
        EQUALITY,
        EXPRESSION,
        FALLTHROUGH,
        FOR,
        FOR_IN,
        FUNC,
        GOTO,
        IDENTIFIER,
        IF,
        IMPL,
        IMPORT,
        IMPORT_FROM,
        IMPORT_NAME,
        INDEX,
        LABEL,
        LIST,
        LITERAL,
        LOGICAL,
        LOOP,
        MAP,
        MEMBER,
        NULLABLE,
        PROGRAM,
        RELATIONAL,
        RETURN,
        SCOPE,
        SET,
        SLICE,
        SPAWN,
        STRUCT,
        STRUCT_INIT,
        SUBSCRIPT,
        SWITCH,
        TEST,
        TRAIT,
        TUPLE,
        UNARY_OP,
        UPDATE,
        VARIABLE,
    };

    struct Node {
        NodeType type;
        scanner::Pos start = 0;
        scanner::Pos end = 0;

        explicit Node(NodeType type, scanner::Pos start, scanner::Pos end) : type(type), start(start), end(end) {}

        virtual ~Node() = default;

        virtual std::string String() { return ""; }
    };

    struct NodeDoc : Node {
        std::list<struct Comment> docs;

        explicit NodeDoc(NodeType type, scanner::Pos start, scanner::Pos end) : Node(type, start, end) {}
    };

    using NodeUptr = std::unique_ptr<const Node>;

    template<typename T>
    constexpr T *CastNode(const NodeUptr &node) {
        return ((T *) node.get());
    }

    // **********************************************
    // NODES
    // **********************************************

    struct Alias : Node {
        NodeUptr name;
        NodeUptr value;
        bool pub;

        explicit Alias(NodeUptr name, NodeUptr value, scanner::Pos start, scanner::Pos end) : Alias(std::move(name),
                                                                                                    std::move(value),
                                                                                                    false, start,
                                                                                                    end) {}

        explicit Alias(NodeUptr name, NodeUptr value, bool pub, scanner::Pos start, scanner::Pos end) : Node(
                NodeType::ALIAS, start, end) {
            this->name = std::move(name);
            this->value = std::move(value);
            this->pub = pub;
        }
    };

    struct Assignment : Node {
        NodeUptr assignee;
        NodeUptr right;
        scanner::TokenType kind;

        explicit Assignment(scanner::TokenType kind, NodeUptr assignee, NodeUptr right) : Node(NodeType::ASSIGN, 0, 0) {
            this->kind = kind;
            this->assignee = std::move(assignee);
            this->right = std::move(right);
            this->start = this->assignee->start;
            this->end = this->right->end;
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

    struct Block : Node {
        std::list<NodeUptr> stmts;

        Block(NodeType type, scanner::Pos start) : Node(type, start, 0) {}

        void AddStmtOrExpr(NodeUptr stmt) {
            this->stmts.push_back(std::move(stmt));
        }

        void SetEndPos(scanner::Pos end) {
            this->end = end;
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

    struct Case : Node {
        std::list<NodeUptr> tests;
        NodeUptr body;

        explicit Case(scanner::Pos start) : Node(NodeType::CASE, start, 0) {}

        void AddCondition(NodeUptr condition) {
            this->tests.push_back(std::move(condition));
        }
    };

    struct Comment : Node {
        const std::string comment;

        explicit Comment(const scanner::Token &token) : Node(NodeType::COMMENT, token.start, token.end),
                                                        comment(token.value) {}
    };

    struct Constant : Node {
        bool pub;
        std::string name;
        NodeUptr value;

        explicit Constant(std::string &name, NodeUptr value, bool pub, scanner::Pos start) : Node(NodeType::CONSTANT,
                                                                                                  start, 0) {
            this->pub = pub;
            this->name = name;
            this->value = std::move(value);
            this->end = this->value->end;
        }
    };

    struct Construct : NodeDoc {
        std::string name;
        std::list<NodeUptr> impls;
        NodeUptr body;
        bool pub = false;

        explicit Construct(NodeType type, std::string &name, std::list<NodeUptr> &impls, NodeUptr body, bool pub,
                           scanner::Pos start) : NodeDoc(type, start, 0) {
            this->name = name;
            this->impls = std::move(impls);
            this->body = std::move(body);
            this->pub = pub;
            this->end = this->body->end;
        }
    };

    struct For : Node {
        NodeUptr init;
        NodeUptr test;
        NodeUptr inc;
        NodeUptr body;

        explicit For(NodeType type, NodeUptr iter, NodeUptr expr, NodeUptr inc, NodeUptr body, scanner::Pos start)
                : Node(type, start, 0) {
            this->init = std::move(iter);
            this->test = std::move(expr);
            this->inc = std::move(inc);
            this->body = std::move(body);
            this->end = this->body->end;
        }
    };

    struct Function : NodeDoc {
        std::string id;
        std::list<NodeUptr> params;
        NodeUptr body;
        bool pub = false;

        explicit Function(std::string &id, std::list<NodeUptr> params, NodeUptr body, bool pub, scanner::Pos start)
                : NodeDoc(NodeType::FUNC, start, 0) {
            this->id = id;
            this->params = std::move(params);
            this->body = std::move(body);
            this->pub = pub;
            this->end = this->body->end;
        }

        explicit Function(std::list<NodeUptr> params, NodeUptr body, scanner::Pos start) : NodeDoc(NodeType::FUNC,
                                                                                                   start, 0) {
            this->params = std::move(params);
            this->body = std::move(body);
            this->end = this->body->end;
        }
    };

    struct Identifier : Node {
        std::string value;
        bool rest_element;

        explicit Identifier(const scanner::Token &token) : Identifier(token, false) {}

        explicit Identifier(const scanner::Token &token, bool rest_element) : Node(NodeType::IDENTIFIER, token.start,
                                                                                   token.end) {
            this->value = token.value;
            this->rest_element = rest_element;
        }
    };

    struct If : Node {
        NodeUptr test;
        NodeUptr body;
        NodeUptr orelse;

        explicit If(NodeUptr test, NodeUptr body, NodeUptr orelse, scanner::Pos start, scanner::Pos end) : If(
                std::move(test), std::move(body), start) {
            this->orelse = std::move(orelse);
            this->end = end;
        }

        explicit If(NodeUptr test, NodeUptr body, scanner::Pos start) : Node(NodeType::IF, start, 0) {
            this->test = std::move(test);
            this->body = std::move(body);
            this->end = this->body->end;
        }
    };

    struct Impl : Node {
        NodeUptr target;
        NodeUptr trait;
        NodeUptr block;

        explicit Impl(NodeUptr target, NodeUptr trait, NodeUptr block, scanner::Pos start) : Node(NodeType::IMPL, start,
                                                                                                  0) {
            this->target = std::move(target);
            this->trait = std::move(trait);
            this->block = std::move(block);
            this->end = this->block->end;
        }

        explicit Impl(NodeUptr target, NodeUptr block, scanner::Pos start) : Impl(std::move(target), nullptr,
                                                                                  std::move(block), start) {}
    };

    struct Import : Node {
        NodeUptr module;
        std::list<NodeUptr> names;

        explicit Import(scanner::Pos start) : Node(NodeType::IMPORT, start, 0) {}

        explicit Import(NodeUptr module, scanner::Pos start) : Node(NodeType::IMPORT_FROM, start, 0) {
            this->module = std::move(module);
        }

        void AddName(NodeUptr name) {
            this->end = name->end;
            this->names.push_back(std::move(name));
        }
    };

    struct ImportName : Node {
        std::string name;
        std::string import_as;

        explicit ImportName(scanner::Pos start) : Node(NodeType::IMPORT_NAME, start, 0) {}

        void AddSegment(const std::string segment, scanner::Pos end) {
            if (!this->name.empty()) {
                this->name += "::" + segment;
                this->import_as = segment;
            } else {
                name = segment;
                import_as = segment;
            }
            this->end = end;
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

    struct Literal : Node {
        scanner::TokenType kind;
        std::string value;

        explicit Literal(const scanner::Token &token) : Node(NodeType::LITERAL, token.start, token.end) {
            this->kind = token.type;
            this->value = token.value;
        }
    };

    struct Loop : Node {
        NodeUptr test;
        NodeUptr body;

        explicit Loop(NodeUptr body, scanner::Pos start) : Loop(nullptr, std::move(body), start) {}

        explicit Loop(NodeUptr test, NodeUptr body, scanner::Pos start) : Node(NodeType::LOOP, start, 0) {
            this->test = std::move(test);
            this->body = std::move(body);
            this->end = this->body->end;
        }
    };

    struct Member : Node {
        NodeUptr left;
        NodeUptr right;
        bool safe;

        explicit Member(NodeUptr left, NodeUptr right, bool safe) : Node(NodeType::MEMBER, left->start, right->end),
                                                                    left(std::move(left)), right(std::move(right)),
                                                                    safe(safe) {}
    };

    struct Program : NodeDoc {
        std::list<NodeUptr> body;
        std::string filename;

        explicit Program(std::string &filename, scanner::Pos start) : NodeDoc(NodeType::PROGRAM, start, 0) {
            this->filename = filename;
        }

        void AddStatement(NodeUptr statement) {
            this->body.push_back(std::move(statement));
        }

        void SetEndPos(scanner::Pos end) {
            this->end = end;
        }
    };

    struct Scope : Node {
        std::list<std::string> segments;

        explicit Scope(scanner::Pos start) : Node(NodeType::SCOPE, start, 0) {}

        void AddSegment(const std::string &segment) {
            this->segments.push_back(segment);
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

    struct StructInit : Node {
        NodeUptr left;
        std::list<NodeUptr> args;
        bool keys = false;

        explicit StructInit(NodeUptr left) : Node(NodeType::STRUCT_INIT, 0, 0) {
            this->left = std::move(left);
            this->start = this->left->start;
        }

        void AddArgument(NodeUptr arg) {
            this->args.push_back(std::move(arg));
        }

        void AddKeyValue(NodeUptr key, NodeUptr value) {
            this->args.push_back(std::move(key));
            this->args.push_back(std::move(value));
            this->keys = true;
        }
    };

    struct Switch : Node {
        NodeUptr test;
        std::list<NodeUptr> cases;

        explicit Switch(NodeUptr test, scanner::Pos start) : Node(NodeType::SWITCH, start, 0) {
            this->test = std::move(test);
        }

        void AddCase(NodeUptr swcase) {
            this->cases.push_back(std::move(swcase));
        }
    };

    struct Unary : Node {
        NodeUptr expr;
        lang::scanner::TokenType kind;

        explicit Unary(NodeType type, lang::scanner::TokenType kind, NodeUptr expr, scanner::Pos start,
                       scanner::Pos end) : Node(type, start, end) {
            this->expr = std::move(expr);
            this->kind = kind;
        }

        explicit Unary(NodeType type, NodeUptr expr, scanner::Pos start) : Node(type, start, 0) {
            this->expr = std::move(expr);
            if (this->expr)
                this->end = this->expr->end;
            this->kind = scanner::TokenType::TK_NULL;
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

    struct Variable : Node {
        bool atomic = false;
        bool weak = false;
        bool pub;
        std::string name;
        NodeUptr value;
        NodeUptr annotation;

        explicit Variable(std::string &name, bool pub, scanner::Pos start) : Node(NodeType::VARIABLE, start, 0) {
            this->pub = pub;
            this->name = name;
        }
    };

} // namespace lang::ast

#endif // !ARGON_LANG_AST_AST_H_
