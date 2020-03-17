// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "compiler.h"
#include "parser.h"

using namespace lang;
using namespace lang::ast;

void Compiler::EmitOp(OpCodes code, unsigned char arg) {
    //if(arg > 0x00FFFFFF)
    //    throw
    // TODO: bad arg, argument too long!
    // TODO: emit bytecode!
    this->bb_curr_->AddInstr(((InstrSz) code << (unsigned char) 24) | arg);
}

void Compiler::CompileBinaryExpr(const ast::Binary *binary) {
    switch (binary->kind) {
        case scanner::TokenType::SHL:
            this->EmitOp(OpCodes::SHL, 0);
            return;
        case scanner::TokenType::SHR:
            this->EmitOp(OpCodes::SHR, 0);
            return;
        case scanner::TokenType::PLUS:
            this->EmitOp(OpCodes::ADD, 0);
            return;
        case scanner::TokenType::MINUS:
            this->EmitOp(OpCodes::SUB, 0);
            return;
        case scanner::TokenType::ASTERISK:
            this->EmitOp(OpCodes::MUL, 0);
            return;
        case scanner::TokenType::SLASH:
            this->EmitOp(OpCodes::DIV, 0);
            return;
        case scanner::TokenType::SLASH_SLASH:
            this->EmitOp(OpCodes::IDIV, 0);
            return;
        case scanner::TokenType::PERCENT:
            this->EmitOp(OpCodes::MOD, 0);
            return;
        default:
            assert(false);
    }
}

void Compiler::CompileBranch(const ast::If *stmt) {
    BasicBlock *test_block;
    BasicBlock *true_block;

    this->CompileCode(stmt->test);
    this->EmitOp(OpCodes::JF, 0);

    test_block = this->NewNextBlock();

    this->CompileCode(stmt->body);
    if (stmt->orelse != nullptr)
        this->EmitOp(OpCodes::JMP, 0);
    true_block = this->NewNextBlock();

    test_block->flow_else = this->bb_curr_;
    if (stmt->orelse != nullptr) {
        this->CompileCode(stmt->orelse);
        this->NewNextBlock();
        true_block->flow_next = nullptr;
        true_block->flow_else = this->bb_curr_;
    }
}

void Compiler::CompileCode(const ast::NodeUptr &node) {
    BasicBlock *tmp;

    switch (node->type) {
        case NodeType::BLOCK:
            for (auto &stmt : CastNode<Block>(node)->stmts)
                this->CompileCode(stmt);
            break;
        case NodeType::IF:
            this->CompileBranch(CastNode<If>(node));
            break;
        case NodeType::ELVIS:
            this->CompileCode(CastNode<Binary>(node)->left);
            this->EmitOp(OpCodes::JTOP, 0);
            tmp = this->NewNextBlock();
            this->CompileCode(CastNode<Binary>(node)->right);
            this->NewNextBlock();
            tmp->flow_else = this->bb_curr_;
            break;
        case NodeType::TEST:
            this->CompileTest(CastNode<Binary>(node));
            break;
        case NodeType::LOGICAL:
            this->CompileCode(CastNode<Binary>(node)->left);
            this->CompileCode(CastNode<Binary>(node)->right);
            if (CastNode<Binary>(node)->kind == scanner::TokenType::PIPE)
                this->EmitOp(OpCodes::LOR, 0);
            else if (CastNode<Binary>(node)->kind == scanner::TokenType::CARET)
                this->EmitOp(OpCodes::LXOR, 0);
            else this->EmitOp(OpCodes::LAND, 0);
            break;
        case NodeType::EQUALITY:
            this->CompileCode(CastNode<Binary>(node)->left);
            this->CompileCode(CastNode<Binary>(node)->right);
            if (CastNode<Binary>(node)->kind == scanner::TokenType::EQUAL_EQUAL)
                this->EmitOp(OpCodes::CMP, (unsigned char) CompareMode::EQ);
            else if (CastNode<Binary>(node)->kind == scanner::TokenType::NOT_EQUAL)
                this->EmitOp(OpCodes::CMP, (unsigned char) CompareMode::NE);
            break;
        case NodeType::RELATIONAL:
            this->CompileCode(CastNode<Binary>(node)->left);
            this->CompileCode(CastNode<Binary>(node)->right);
            if (CastNode<Binary>(node)->kind == scanner::TokenType::GREATER)
                this->EmitOp(OpCodes::CMP, (unsigned char) CompareMode::GE);
            else if (CastNode<Binary>(node)->kind == scanner::TokenType::GREATER_EQ)
                this->EmitOp(OpCodes::CMP, (unsigned char) CompareMode::GEQ);
            else if (CastNode<Binary>(node)->kind == scanner::TokenType::LESS)
                this->EmitOp(OpCodes::CMP, (unsigned char) CompareMode::LE);
            else if (CastNode<Binary>(node)->kind == scanner::TokenType::LESS_EQ)
                this->EmitOp(OpCodes::CMP, (unsigned char) CompareMode::LEQ);
            break;
        case NodeType::BINARY_OP:
            this->CompileCode(CastNode<Binary>(node)->left);
            this->CompileCode(CastNode<Binary>(node)->right);
            this->CompileBinaryExpr(CastNode<Binary>(node));
            break;
        case NodeType::UNARY_OP:
            this->CompileCode(CastNode<Unary>(node)->expr);
            this->CompileUnaryExpr(CastNode<Unary>(node));
            break;
        case NodeType::IDENTIFIER:
            break;
        case NodeType::SCOPE:
            break;
        case NodeType::LITERAL:
            this->CompileLiteral(CastNode<Literal>(node));
            break;
        default:
            assert(false);
    }
}

void Compiler::CompileLiteral(const ast::Literal *literal) {
    // TODO: impl literal
}

void Compiler::CompileTest(const ast::Binary *test) {
    BasicBlock *last = this->NewBlock();
    BasicBlock *left;

    while (true) {
        this->CompileCode(test->left);
        if (test->kind == scanner::TokenType::AND)
            this->EmitOp(OpCodes::JFOP, 0);
        else
            this->EmitOp(OpCodes::JTOP, 0);

        left = this->NewNextBlock();
        left->flow_else = last;

        if (test->right->type != NodeType::TEST)
            break;

        test = CastNode<Binary>(test->right);
    }

    this->CompileCode(test->right);
    this->UseAsNextBlock(last);
}

void Compiler::CompileUnaryExpr(const ast::Unary *unary) {
    switch (unary->kind) {
        case scanner::TokenType::EXCLAMATION:
            this->EmitOp(OpCodes::UNARY_NOT, 0);
            return;
        case scanner::TokenType::TILDE:
            this->EmitOp(OpCodes::UNARY_INV, 0);
            return;
        case scanner::TokenType::PLUS:
            this->EmitOp(OpCodes::UNARY_POS, 0);
            return;
        case scanner::TokenType::MINUS:
            this->EmitOp(OpCodes::UNARY_NEG, 0);
            return;
        case scanner::TokenType::PLUS_PLUS:
            this->EmitOp(OpCodes::PREFX_INC, 0);
            return;
        case scanner::TokenType::MINUS_MINUS:
            this->EmitOp(OpCodes::PREFX_DEC, 0);
            return;
        default:
            assert(false);
    }
}

void Compiler::UseAsNextBlock(BasicBlock *block) {
    this->bb_curr_->flow_next = block;
    this->bb_curr_ = block;
}

BasicBlock *Compiler::NewBlock() {
    auto bb = new BasicBlock();
    bb->link_next = this->bb_list;
    this->bb_list = bb;
    if (this->bb_start_ == nullptr)
        this->bb_start_ = bb;
    return bb;
}

BasicBlock *Compiler::NewNextBlock() {
    BasicBlock *next = this->NewBlock();
    BasicBlock *prev = this->bb_curr_;

    if (this->bb_curr_ != nullptr)
        this->bb_curr_->flow_next = next;
    this->bb_curr_ = next;

    return prev;
}

void Compiler::Compile(std::istream *source) {
    Parser parser(source);
    std::unique_ptr<ast::Program> program = parser.Parse();

    this->NewNextBlock();

    for (auto &stmt : program->body)
        this->CompileCode(stmt);
}
