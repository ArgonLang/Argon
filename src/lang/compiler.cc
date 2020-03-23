// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include <object/string.h>
#include <object/integer.h>
#include <object/decimal.h>
#include <object/nil.h>
#include <object/bool.h>

#include "compiler.h"
#include "parser.h"

using namespace lang;
using namespace lang::ast;
using namespace argon::object;

void Compiler::EmitOp(OpCodes code, unsigned char arg) {
    //if(arg > 0x00FFFFFF)
    //    throw
    // TODO: bad arg, argument too long!
    // TODO: emit bytecode!
    this->cu_curr_->bb_curr->AddInstr(((InstrSz) code << (unsigned char) 24) | arg);
}

void Compiler::EnterScope() {
    // TODO: fix this! it's only a template for dirty experiments
    CompileUnit *cu = &this->cu_list_.emplace_back();
    cu->symt = std::make_unique<SymbolTable>("", SymTScope::MODULE);
    cu->prev = this->cu_curr_;
    this->cu_curr_ = cu;
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

    test_block->flow_else = this->cu_curr_->bb_curr;
    if (stmt->orelse != nullptr) {
        this->CompileCode(stmt->orelse);
        this->NewNextBlock();
        true_block->flow_next = nullptr;
        true_block->flow_else = this->cu_curr_->bb_curr;
    }
}

void Compiler::CompileCode(const ast::NodeUptr &node) {
    BasicBlock *tmp;

    switch (node->type) {
        case NodeType::VARIABLE:
            this->CompileVariable(CastNode<Variable>(node));
            break;
        case NodeType::BLOCK:
            for (auto &stmt : CastNode<Block>(node)->stmts)
                this->CompileCode(stmt);
            break;
        case NodeType::LOOP:
            this->CompileLoop(CastNode<Loop>(node));
            break;
        case NodeType::SWITCH:
            this->CompileSwitch(CastNode<Switch>(node));
            break;
        case NodeType::FALLTHROUGH:
            //  Managed by CompileSwitch* method
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
            tmp->flow_else = this->cu_curr_->bb_curr;
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
        case NodeType::UPDATE:
            this->CompileCode(CastNode<Update>(node)->expr);
            if (CastNode<Update>(node)->kind == scanner::TokenType::PLUS_PLUS)
                this->EmitOp(CastNode<Update>(node)->prefix ? OpCodes::PREI : OpCodes::PSTI, 0);
            else // TokenType::MINUS_MINUS
                this->EmitOp(CastNode<Update>(node)->prefix ? OpCodes::PRED : OpCodes::PSTD, 0);
            break;
        case NodeType::TUPLE:
        case NodeType::LIST:
        case NodeType::SET:
        case NodeType::MAP:
            this->CompileCompound(CastNode<List>(node));
            break;
        case NodeType::IDENTIFIER:
            this->CompileIdentifier(CastNode<Identifier>(node));
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

void Compiler::CompileLoop(const ast::Loop *loop) {
    BasicBlock *next = this->NewBlock();
    BasicBlock *test = nullptr;

    this->NewNextBlock();
    if (loop->test != nullptr) {
        this->CompileCode(loop->test);
        this->EmitOp(OpCodes::JF, 0);
        test = this->NewNextBlock();
        test->flow_else = next;
        this->cu_curr_->bb_curr->flow_else = test;
    } else this->cu_curr_->bb_curr->flow_else = this->cu_curr_->bb_curr;
    this->CompileCode(loop->body);
    this->EmitOp(OpCodes::JMP, 0);

    this->UseAsNextBlock(next);
}

void Compiler::CompileCompound(const ast::List *list) {
    for (auto &expr : list->expressions)
        this->CompileCode(expr);

    switch (list->type) {
        case NodeType::TUPLE:
            this->EmitOp(OpCodes::MK_TUPLE, list->expressions.size());
            return;
        case NodeType::LIST:
            this->EmitOp(OpCodes::MK_LIST, list->expressions.size());
            return;
        case NodeType::SET:
            this->EmitOp(OpCodes::MK_SET, list->expressions.size());
            return;
        case NodeType::MAP:
            this->EmitOp(OpCodes::MK_MAP, list->expressions.size() / 2);
            return;
        default:
            assert(false);
    }
}

void Compiler::CompileSwitch(const ast::Switch *stmt) {
    BasicBlock *last = this->NewBlock();
    BasicBlock *bcur = nullptr;
    BasicBlock *cond = nullptr;
    unsigned short index = 1;
    bool have_default = false;

    this->CompileCode(stmt->test);

    for (auto &swcase : stmt->cases) {
        bcur = this->cu_curr_->bb_curr;
        this->cu_curr_->bb_curr = this->NewBlock();
        this->CompileCode(CastNode<Case>(swcase)->body);

        if (index < stmt->cases.size()) {
            if (CastNode<Block>(CastNode<Case>(swcase)->body)->stmts.back()->type != NodeType::FALLTHROUGH) {
                this->EmitOp(OpCodes::JMP, 0);
                this->cu_curr_->bb_curr->flow_else = last;
            }
        }

        if (cond != nullptr)
            cond->flow_next = this->cu_curr_->bb_curr;

        cond = this->cu_curr_->bb_curr;

        this->cu_curr_->bb_curr = bcur;
        if (!CastNode<Case>(swcase)->tests.empty()) {
            for (auto &test : CastNode<Case>(swcase)->tests) {
                this->CompileCode(test);
                this->EmitOp(OpCodes::TEST, 0);
                this->cu_curr_->bb_curr->flow_else = cond;
                this->NewNextBlock();
            }
        } else {
            // Default branch:
            this->EmitOp(OpCodes::JMP, 0);
            this->cu_curr_->bb_curr->flow_else = cond;
            have_default = true;
        }

        index++;
    }

    if (!have_default) {
        this->EmitOp(OpCodes::JMP, 0);
        this->cu_curr_->bb_curr->flow_else = last;
    }

    this->cu_curr_->bb_curr = cond;

    this->UseAsNextBlock(last);
}

void Compiler::CompileVariable(const ast::Variable *variable) {
    Symbol *sym = this->cu_curr_->symt->Insert(variable->name);
    bool unbound = true;

    if (sym == nullptr) {
        sym = this->cu_curr_->symt->Lookup(variable->name);
        if (sym->declared)
            return; // TODO exception!
        unbound = false;
    }
    sym->declared = true;

    if (this->cu_curr_->symt->type != SymTScope::MODULE) {
        sym->id = this->cu_curr_->locals.size();
        this->cu_curr_->locals.push_back(variable->name);
        this->EmitOp(OpCodes::STLC, sym->id);
        return;
    }

    if (unbound) {
        sym->id = this->cu_curr_->names.size();
        this->cu_curr_->names.push_back(variable->name);
    }
    this->EmitOp(OpCodes::STGBL, sym->id);
}

void Compiler::CompileIdentifier(const ast::Identifier *identifier) {
    Symbol *sym = this->cu_curr_->symt->Lookup(identifier->value);

    if (sym == nullptr) {
        sym = this->cu_curr_->symt->Insert(identifier->value);
        sym->id = this->cu_curr_->names.size();
        this->cu_curr_->names.push_back(identifier->value);
        this->EmitOp(OpCodes::LDGBL, sym->id);
        return;
    }

    if (this->cu_curr_->symt->level == sym->table->level && this->cu_curr_->symt->type == SymTScope::FUNCTION) {
        this->EmitOp(OpCodes::LDLC, sym->id);
        return;
    }

    this->EmitOp(OpCodes::LDGBL, sym->id);
}

void Compiler::CompileLiteral(const ast::Literal *literal) {
    // TODO: implement flyweight
    switch (literal->kind) {
        case scanner::TokenType::NIL:
            this->cu_curr_->constant.emplace_back(Nil::NilValue());
            break;
        case scanner::TokenType::FALSE:
            this->cu_curr_->constant.emplace_back(Bool::False());
            break;
        case scanner::TokenType::TRUE:
            this->cu_curr_->constant.emplace_back(Bool::True());
            break;
        case scanner::TokenType::STRING:
            this->cu_curr_->constant.emplace_back(MakeOwner<String>(literal->value));
            break;
        case scanner::TokenType::NUMBER_BIN:
            this->cu_curr_->constant.emplace_back(MakeOwner<Integer>(literal->value, 2));
            break;
        case scanner::TokenType::NUMBER_OCT:
            this->cu_curr_->constant.emplace_back(MakeOwner<Integer>(literal->value, 8));
            break;
        case scanner::TokenType::NUMBER:
            this->cu_curr_->constant.emplace_back(MakeOwner<Integer>(literal->value, 10));
            break;
        case scanner::TokenType::NUMBER_HEX:
            this->cu_curr_->constant.emplace_back(MakeOwner<Integer>(literal->value, 16));
            break;
        case scanner::TokenType::DECIMAL:
            this->cu_curr_->constant.emplace_back(MakeOwner<Decimal>(literal->value));
            break;
        default:
            assert(false);
    }

    this->EmitOp(OpCodes::LSTATIC, this->cu_curr_->constant.size() - 1);
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
            this->EmitOp(OpCodes::NOT, 0);
            return;
        case scanner::TokenType::TILDE:
            this->EmitOp(OpCodes::INV, 0);
            return;
        case scanner::TokenType::PLUS:
            this->EmitOp(OpCodes::POS, 0);
            return;
        case scanner::TokenType::MINUS:
            this->EmitOp(OpCodes::NEG, 0);
            return;
        default:
            assert(false);
    }
}

void Compiler::UseAsNextBlock(BasicBlock *block) {
    this->cu_curr_->bb_curr->flow_next = block;
    this->cu_curr_->bb_curr = block;
}

BasicBlock *Compiler::NewBlock() {
    auto bb = new BasicBlock();
    bb->link_next = this->cu_curr_->bb_list;
    this->cu_curr_->bb_list = bb;
    if (this->cu_curr_->bb_start == nullptr)
        this->cu_curr_->bb_start = bb;
    return bb;
}

BasicBlock *Compiler::NewNextBlock() {
    BasicBlock *next = this->NewBlock();
    BasicBlock *prev = this->cu_curr_->bb_curr;

    if (this->cu_curr_->bb_curr != nullptr)
        this->cu_curr_->bb_curr->flow_next = next;
    this->cu_curr_->bb_curr = next;

    return prev;
}

void Compiler::Compile(std::istream *source) {
    Parser parser(source);
    std::unique_ptr<ast::Program> program = parser.Parse();

    this->EnterScope();

    this->NewNextBlock();

    for (auto &stmt : program->body)
        this->CompileCode(stmt);
}
