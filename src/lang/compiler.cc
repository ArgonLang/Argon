// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include <object/string.h>
#include <object/integer.h>
#include <object/decimal.h>
#include <object/nil.h>
#include <object/bool.h>
#include <object/code.h>

#include "compiler.h"
#include "parser.h"

using namespace lang;
using namespace lang::ast;
using namespace argon::object;

void Compiler::EmitOp(OpCodes code) {
    this->cu_curr_->bb_curr->AddInstr((Instr8) code);
}

void Compiler::EmitOp2(OpCodes code, unsigned char arg) {
    this->cu_curr_->bb_curr->AddInstr((Instr16) ((unsigned short) (arg << (unsigned char) 8) | (Instr8) code));
}

void Compiler::EmitOp4(OpCodes code, unsigned int arg) {
    //if(arg > 0x00FFFFFF)
    //    throw
    // TODO: bad arg, argument too long!
    this->cu_curr_->bb_curr->AddInstr((Instr32) (arg << (unsigned char) 8) | (Instr8) code);
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
            this->EmitOp(OpCodes::SHL);
            return;
        case scanner::TokenType::SHR:
            this->EmitOp(OpCodes::SHR);
            return;
        case scanner::TokenType::PLUS:
            this->EmitOp(OpCodes::ADD);
            return;
        case scanner::TokenType::MINUS:
            this->EmitOp(OpCodes::SUB);
            return;
        case scanner::TokenType::ASTERISK:
            this->EmitOp(OpCodes::MUL);
            return;
        case scanner::TokenType::SLASH:
            this->EmitOp(OpCodes::DIV);
            return;
        case scanner::TokenType::SLASH_SLASH:
            this->EmitOp(OpCodes::IDIV);
            return;
        case scanner::TokenType::PERCENT:
            this->EmitOp(OpCodes::MOD);
            return;
        default:
            assert(false);
    }
}

void Compiler::CompileBranch(const ast::If *stmt) {
    BasicBlock *test = this->cu_curr_->bb_curr;
    BasicBlock *last = this->NewBlock();

    this->CompileCode(stmt->test);
    this->EmitOp4(OpCodes::JF, 0);
    test->flow_else = last;

    this->NewNextBlock();
    this->CompileCode(stmt->body);
    if (stmt->orelse != nullptr) {
        this->cu_curr_->bb_curr->flow_else = last;
        this->EmitOp4(OpCodes::JMP, 0);
        this->NewNextBlock();
        test->flow_else = this->cu_curr_->bb_curr;
        this->CompileCode(stmt->orelse);
    }

    this->UseAsNextBlock(last);
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
            if (CastNode<Switch>(node)->test != nullptr)
                this->CompileSwitch(CastNode<Switch>(node));
            else
                this->CompileSwitchAsIf(CastNode<Switch>(node));
            break;
        case NodeType::FALLTHROUGH:
            //  Managed by CompileSwitch* method
            break;
        case NodeType::IF:
            this->CompileBranch(CastNode<If>(node));
            break;
        case NodeType::ELVIS:
            this->CompileCode(CastNode<Binary>(node)->left);
            this->EmitOp4(OpCodes::JTOP, 0);
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
                this->EmitOp(OpCodes::LOR);
            else if (CastNode<Binary>(node)->kind == scanner::TokenType::CARET)
                this->EmitOp(OpCodes::LXOR);
            else this->EmitOp(OpCodes::LAND);
            break;
        case NodeType::EQUALITY:
            this->CompileCode(CastNode<Binary>(node)->left);
            this->CompileCode(CastNode<Binary>(node)->right);
            if (CastNode<Binary>(node)->kind == scanner::TokenType::EQUAL_EQUAL)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::EQ);
            else if (CastNode<Binary>(node)->kind == scanner::TokenType::NOT_EQUAL)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::NE);
            break;
        case NodeType::RELATIONAL:
            this->CompileCode(CastNode<Binary>(node)->left);
            this->CompileCode(CastNode<Binary>(node)->right);
            if (CastNode<Binary>(node)->kind == scanner::TokenType::GREATER)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::GE);
            else if (CastNode<Binary>(node)->kind == scanner::TokenType::GREATER_EQ)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::GEQ);
            else if (CastNode<Binary>(node)->kind == scanner::TokenType::LESS)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::LE);
            else if (CastNode<Binary>(node)->kind == scanner::TokenType::LESS_EQ)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::LEQ);
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
                this->EmitOp(CastNode<Update>(node)->prefix ? OpCodes::PREI : OpCodes::PSTI);
            else // TokenType::MINUS_MINUS
                this->EmitOp(CastNode<Update>(node)->prefix ? OpCodes::PRED : OpCodes::PSTD);
            break;
        case NodeType::TUPLE:
        case NodeType::LIST:
        case NodeType::SET:
        case NodeType::MAP:
            this->CompileCompound(CastNode<ast::List>(node));
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
    BasicBlock *last = this->NewBlock();
    BasicBlock *head = nullptr;

    this->NewNextBlock();
    head = this->cu_curr_->bb_curr;
    if (loop->test != nullptr) {
        this->CompileCode(loop->test);
        this->EmitOp4(OpCodes::JF, 0);
        this->cu_curr_->bb_curr->flow_else = last;
        this->NewNextBlock();
    }
    this->CompileCode(loop->body);
    this->EmitOp4(OpCodes::JMP, 0);
    this->cu_curr_->bb_curr->flow_else = head;
    this->UseAsNextBlock(last);
}

void Compiler::CompileCompound(const ast::List *list) {
    for (auto &expr : list->expressions)
        this->CompileCode(expr);

    switch (list->type) {
        case NodeType::TUPLE:
            this->EmitOp4(OpCodes::MK_TUPLE, list->expressions.size());
            return;
        case NodeType::LIST:
            this->EmitOp4(OpCodes::MK_LIST, list->expressions.size());
            return;
        case NodeType::SET:
            this->EmitOp4(OpCodes::MK_SET, list->expressions.size());
            return;
        case NodeType::MAP:
            this->EmitOp4(OpCodes::MK_MAP, list->expressions.size() / 2);
            return;
        default:
            assert(false);
    }
}

void Compiler::CompileSwitch(const ast::Switch *stmt) {
    BasicBlock *last = this->NewBlock();
    BasicBlock *cond = this->cu_curr_->bb_curr;
    BasicBlock *fbody = this->NewBlock();
    BasicBlock *body = fbody;
    BasicBlock *def = nullptr;
    unsigned short index = 1;

    this->CompileCode(stmt->test);

    for (auto &swcase : stmt->cases) {
        if (index != 1) {
            body->flow_next = this->NewBlock();
            body = body->flow_next;
        }

        if (!CastNode<Case>(swcase)->tests.empty()) {
            // Case
            bool compound_cond = false;
            for (auto &test : CastNode<Case>(swcase)->tests) {
                if (compound_cond || cond->flow_else != nullptr) {
                    this->NewNextBlock();
                    cond = this->cu_curr_->bb_curr;
                }
                compound_cond = true;
                this->CompileCode(test);
                this->EmitOp(OpCodes::TEST);
                this->EmitOp4(OpCodes::JTAP, 0);
                cond->flow_else = body;
            }
        } else {
            // Default
            def = this->NewBlock();
            this->cu_curr_->bb_curr = def;
            this->EmitOp4(OpCodes::JMP, 0);
            this->cu_curr_->bb_curr->flow_else = body;
        }

        this->cu_curr_->bb_curr = body; // Use bb pointed by body
        this->CompileCode(CastNode<Case>(swcase)->body);
        body = this->cu_curr_->bb_curr; // Update body after CompileCode, body may be changed (Eg: if)

        if (index < stmt->cases.size()) {
            if (CastNode<Block>(CastNode<Case>(swcase)->body)->stmts.back()->type != NodeType::FALLTHROUGH) {
                this->EmitOp4(OpCodes::JMP, 0);
                body->flow_else = last;
            }
        }

        this->cu_curr_->bb_curr = cond;
        index++;
    }

    if (def == nullptr) {
        this->NewNextBlock();
        this->EmitOp4(OpCodes::JMP, 0);
        this->cu_curr_->bb_curr->flow_else = last;
    } else this->UseAsNextBlock(def);

    this->cu_curr_->bb_curr->flow_next = fbody;

    this->cu_curr_->bb_curr = body;
    this->UseAsNextBlock(last);
}

/*
 * CompileSwitchAsIf:
 *
 * TEST 1
 * JT BODY1
 * TEST 1.2
 * JT BODY1
 * TEST 1.3
 * JF TEST 2
 * BODY1:
 * ...
 * TEST 2
 * JF EXIT
 * BODY2
 * ...
 * EXIT:
 * ...
 * ...
 */

void Compiler::CompileSwitchAsIf(const ast::Switch *stmt) {
    BasicBlock *last = this->NewBlock();
    BasicBlock *ltest = nullptr;
    BasicBlock *body = nullptr;
    unsigned short cases_idx = 1;

    for (auto &swcase : stmt->cases) {
        body = this->NewBlock();
        unsigned short tests_idx = 1;
        if (!CastNode<Case>(swcase)->tests.empty()) {
            for (auto &test : CastNode<Case>(swcase)->tests) {
                this->CompileCode(test);
                if (tests_idx < CastNode<Case>(swcase)->tests.size()) {
                    this->EmitOp4(OpCodes::JT, 0);
                    this->cu_curr_->bb_curr->flow_else = body;
                    this->NewNextBlock();
                } else this->EmitOp4(OpCodes::JF, 0);
                tests_idx++;
                ltest = this->cu_curr_->bb_curr;
            }
            this->UseAsNextBlock(body);
        }

        this->CompileCode(CastNode<Case>(swcase)->body);

        if (cases_idx < stmt->cases.size()) {
            if (CastNode<Block>(CastNode<Case>(swcase)->body)->stmts.back()->type != NodeType::FALLTHROUGH) {
                this->EmitOp4(OpCodes::JMP, 0);
                this->cu_curr_->bb_curr->flow_else = last;
            }
            this->NewNextBlock();
            ltest->flow_else = this->cu_curr_->bb_curr;
        } else {
            ltest->flow_else = this->cu_curr_->bb_curr;
            this->UseAsNextBlock(last);
        }
        cases_idx++;
    }
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
        this->EmitOp2(OpCodes::STLC, sym->id);
        return;
    }

    if (unbound) {
        sym->id = this->cu_curr_->names->Count();
        this->cu_curr_->names->Append(NewObject<String>(variable->name));
    }
    this->EmitOp2(OpCodes::STGBL, sym->id);
}

void Compiler::CompileIdentifier(const ast::Identifier *identifier) {
    Symbol *sym = this->cu_curr_->symt->Lookup(identifier->value);

    if (sym == nullptr) {
        sym = this->cu_curr_->symt->Insert(identifier->value);
        sym->id = this->cu_curr_->names->Count();
        this->cu_curr_->names->Append(NewObject<String>(identifier->value));
        this->EmitOp2(OpCodes::LDGBL, sym->id);
        return;
    }

    if (this->cu_curr_->symt->level == sym->table->level && this->cu_curr_->symt->type == SymTScope::FUNCTION) {
        this->EmitOp2(OpCodes::LDLC, sym->id);
        return;
    }

    this->EmitOp2(OpCodes::LDGBL, sym->id);
}

void Compiler::CompileLiteral(const ast::Literal *literal) {
    Object *obj;
    Object *tmp;
    unsigned short idx;

    switch (literal->kind) {
        case scanner::TokenType::NIL:
            obj = Nil::NilValue();
            IncStrongRef(obj);
            break;
        case scanner::TokenType::FALSE:
            obj = Bool::False();
            IncStrongRef(obj);
            break;
        case scanner::TokenType::TRUE:
            obj = Bool::True();
            IncStrongRef(obj);
            break;
        case scanner::TokenType::STRING:
            obj = NewObject<String>(literal->value);
            break;
        case scanner::TokenType::NUMBER_BIN:
            obj = NewObject<Integer>(literal->value, 2);
            break;
        case scanner::TokenType::NUMBER_OCT:
            obj = NewObject<Integer>(literal->value, 8);
            break;
        case scanner::TokenType::NUMBER:
            obj = NewObject<Integer>(literal->value, 10);
            break;
        case scanner::TokenType::NUMBER_HEX:
            obj = NewObject<Integer>(literal->value, 16);
            break;
        case scanner::TokenType::DECIMAL:
            obj = NewObject<Decimal>(literal->value);
            break;
        default:
            assert(false);
    }

    tmp = this->cu_curr_->statics_map.GetItem(obj);
    if (IsNil(tmp)) {
        tmp = this->statics_global.GetItem(obj);
        if (!IsNil(tmp)) {
            ReleaseObject(obj);
            obj = tmp;
            IncStrongRef(obj);
        } else
            this->cu_curr_->statics->Append(obj);

        idx = this->cu_curr_->statics->Count() - 1;
        this->cu_curr_->statics_map.Insert(obj, NewObject<Integer>(idx));
        this->statics_global.Insert(obj, obj);

    } else idx = ToCInt(((Integer *) tmp));

    this->EmitOp2(OpCodes::LSTATIC, idx);
    ReleaseObject(obj);
}

void Compiler::CompileTest(const ast::Binary *test) {
    BasicBlock *last = this->NewBlock();
    BasicBlock *left;

    while (true) {
        this->CompileCode(test->left);
        if (test->kind == scanner::TokenType::AND)
            this->EmitOp4(OpCodes::JFOP, 0);
        else
            this->EmitOp4(OpCodes::JTOP, 0);

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
            this->EmitOp(OpCodes::NOT);
            return;
        case scanner::TokenType::TILDE:
            this->EmitOp(OpCodes::INV);
            return;
        case scanner::TokenType::PLUS:
            this->EmitOp(OpCodes::POS);
            return;
        case scanner::TokenType::MINUS:
            this->EmitOp(OpCodes::NEG);
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

    this->Dfs(this->cu_curr_, this->cu_curr_->bb_start); // TODO stub DFS
    this->Assemble();
}

void Compiler::Dfs(CompileUnit *unit, BasicBlock *start) {
    start->visited = true;
    start->start = unit->instr_sz;
    unit->instr_sz += start->instr_sz;
    unit->bb_splist.push_back(start);

    if (start->flow_next != nullptr && !start->flow_next->visited)
        this->Dfs(unit, start->flow_next);

    if (start->flow_else != nullptr && !start->flow_else->visited)
        this->Dfs(unit, start->flow_else);
}

void Compiler::Assemble() {
    auto code = argon::object::NewObject<argon::object::Code>(this->cu_curr_->instr_sz);
    size_t offset = 0;

    for (auto &bb : this->cu_curr_->bb_splist) {
        // Calculate JMP offset
        if (bb->flow_else != nullptr) {
            auto j_off = (OpCodes) (*((Instr32 *) (bb->instrs + (bb->instr_sz - sizeof(Instr32)))) & (Instr8) 0xFF);
            auto jmp = (Instr32) bb->flow_else->start << (unsigned char) 8 | (Instr8) j_off;
            *((Instr32 *) (bb->instrs + (bb->instr_sz - sizeof(Instr32)))) = jmp;
        }
        // Copy instrs to destination CodeObject
        argon::memory::MemoryCopy(code->instr + offset, bb->instrs, bb->instr_sz);
        offset += bb->instr_sz;
    }

    code->statics = this->cu_curr_->statics;
    IncStrongRef(code->statics);

    code->names = this->cu_curr_->names;
    IncStrongRef(code->names);

    argon::memory::FreeObject(code); // TODO: stub (test only)
}
