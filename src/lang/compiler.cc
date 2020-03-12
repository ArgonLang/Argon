// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory>

#include "compiler.h"
#include "parser.h"

using namespace lang;
using namespace lang::ast;

void Compiler::Compile(std::istream *source) {
    Parser parser(source);
    std::unique_ptr<ast::Program> program = parser.Parse();

    for (auto &stmt : program->body)
        this->CompileCode(stmt);
}

void Compiler::CompileCode(const ast::NodeUptr &node) {
    switch (node->type) {
        case NodeType::BINARY_OP:
            if (CastNode<Binary>(node)->kind == scanner::TokenType::ELVIS) {
                this->CompileCode(CastNode<Binary>(node)->right);
                this->CompileCode(CastNode<Binary>(node)->left);
                this->EmitOp(OpCodes::PNOB, 0);
                break;
            }
            this->CompileCode(CastNode<Binary>(node)->left);
            this->CompileCode(CastNode<Binary>(node)->right);
            this->CompileBinaryExpr(CastNode<Binary>(node));
            break;
        case NodeType::UNARY_OP:
            this->CompileCode(CastNode<Unary>(node)->expr);
            if (CastNode<Unary>(node)->kind == scanner::TokenType::EXCLAMATION)
                this->EmitOp(OpCodes::UNARY_NOT, 0);
            else if (CastNode<Unary>(node)->kind == scanner::TokenType::TILDE)
                this->EmitOp(OpCodes::UNARY_INV, 0);
            else if (CastNode<Unary>(node)->kind == scanner::TokenType::PLUS)
                this->EmitOp(OpCodes::UNARY_POS, 0);
            else if (CastNode<Unary>(node)->kind == scanner::TokenType::MINUS)
                this->EmitOp(OpCodes::UNARY_NEG, 0);
            else if (CastNode<Unary>(node)->kind == scanner::TokenType::PLUS_PLUS)
                this->EmitOp(OpCodes::PREFX_INC, 0);
            else if (CastNode<Unary>(node)->kind == scanner::TokenType::MINUS_MINUS)
                this->EmitOp(OpCodes::PREFX_DEC, 0);
            break;
        case NodeType::IDENTIFIER:
            break;
        case NodeType::SCOPE:
            break;
        case NodeType::LITERAL:
            this->CompileLiteral(CastNode<Literal>(node));
            break;
    }
}

void Compiler::CompileLiteral(const ast::Literal *literal) {
    // TODO: impl literal
}

void Compiler::EmitOp(OpCodes code, unsigned char arg) {
    //if(arg > 0x00FFFFFF)
    //    throw
    // TODO: bad arg, argument too long!
    // TODO: emit bytecode!
}

void Compiler::CompileBinaryExpr(const ast::Binary *binary) {
    switch (binary->kind) {
        case scanner::TokenType::EQUAL_EQUAL:
            this->EmitOp(OpCodes::CMP, (unsigned char) CompareMode::EQ);
            return;
        case scanner::TokenType::NOT_EQUAL:
            this->EmitOp(OpCodes::CMP, (unsigned char) CompareMode::NE);
            return;
        case scanner::TokenType::GREATER:
            this->EmitOp(OpCodes::CMP, (unsigned char) CompareMode::GE);
            return;
        case scanner::TokenType::GREATER_EQ:
            this->EmitOp(OpCodes::CMP, (unsigned char) CompareMode::GEQ);
            return;
        case scanner::TokenType::LESS:
            this->EmitOp(OpCodes::CMP, (unsigned char) CompareMode::LE);
            return;
        case scanner::TokenType::LESS_EQ:
            this->EmitOp(OpCodes::CMP, (unsigned char) CompareMode::LEQ);
            return;
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
        case scanner::TokenType::AMPERSAND:
            this->EmitOp(OpCodes::LAND, 0);
            return;
        case scanner::TokenType::CARET:
            this->EmitOp(OpCodes::LXOR, 0);
            return;
        case scanner::TokenType::PIPE:
            this->EmitOp(OpCodes::LOR, 0);
            return;
        default:
            // TODO: && || test
            // Should never get here!
            break;
    }
}
