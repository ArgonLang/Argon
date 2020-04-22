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
#include <object/function.h>

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
    assert(arg < 0x00FFFFFF); // too many argument!
    this->cu_curr_->bb_curr->AddInstr((Instr32) (arg << (unsigned char) 8) | (Instr8) code);
}

void Compiler::EnterScope(const std::string &scope_name, CUScope scope) {
    CompileUnit *cu = &this->cu_list_.emplace_back(scope);
    cu->symt = std::make_unique<SymbolTable>(scope_name);
    cu->prev = this->cu_curr_;
    this->cu_curr_ = cu;
}

void Compiler::ExitScope() {
    this->cu_curr_ = this->cu_curr_->prev;
    this->cu_list_.pop_back();
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
    this->DecEvalStack(1);
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

void Compiler::CompileFunction(const ast::Function *function) {
    argon::object::Code *code;
    argon::object::Function *fn;
    argon::object::ArObject *tmp;
    bool variadic = false;

    this->EnterScope(function->id, CUScope::FUNCTION);
    this->NewNextBlock();

    for (auto &param: function->params) {
        auto id = CastNode<Identifier>(param);
        this->NewVariable(id->value);
        variadic = id->rest_element;
    }

    this->CompileCode(function->body);

    if (CastNode<Block>(function->body)->stmts.empty() ||
        CastNode<Block>(function->body)->stmts.back()->type != NodeType::RETURN)
        this->EmitOp(OpCodes::RET);

    this->Dfs(this->cu_curr_, this->cu_curr_->bb_start);

    code = this->Assemble();

    this->ExitScope();

    fn = FunctionNew(code, function->params.size());
    Release(code);

    if (fn == nullptr)
        throw MemoryException("CompileFunction: FunctionNew");

    fn->variadic = variadic;

    // Push to static resources
    if ((tmp = IntegerNew(this->cu_curr_->statics->len)) == nullptr) {
        Release(fn);
        throw MemoryException("CompileFunction: IntegerNew");
    }

    bool ok = ListAppend(this->cu_curr_->statics, fn);
    Release(tmp);
    Release(fn);

    if (!ok)
        throw MemoryException("CompileFunction: ListAppend(statics)");


    this->EmitOp2(OpCodes::LSTATIC, this->cu_curr_->statics->len - 1);
    this->IncEvalStack();

    if (code->deref->len > 0) {
        for (int i = 0; i < code->deref->len; i++) {
            auto st = (String *) TupleGetItem(code->deref, i);
            this->LoadVariable(std::string((char *) st->buffer, st->len));
            Release(st);
        }
        this->DecEvalStack(code->deref->len);
        this->EmitOp4(OpCodes::MK_CLOSURE, code->deref->len);
    }

    if (!function->id.empty()) {
        this->NewVariable(function->id);
        this->DecEvalStack(1);
    }
}

void Compiler::CompileCode(const ast::NodeUptr &node) {
    BasicBlock *tmp;
    unsigned int stack_sz_tmp;

    switch (node->type) {
        case NodeType::FUNC:
            this->CompileFunction(CastNode<ast::Function>(node));
            break;
        case NodeType::CALL:
            stack_sz_tmp = this->cu_curr_->stack_cu_sz;
            this->CompileCode(CastNode<Call>(node)->callee);
            for (auto &args : CastNode<Call>(node)->args)
                this->CompileCode(args);
            this->cu_curr_->stack_cu_sz = stack_sz_tmp;
            this->EmitOp2(OpCodes::CALL, CastNode<Call>(node)->args.size());
            this->IncEvalStack();
            break;
        case NodeType::RETURN:
            this->CompileCode(CastNode<Unary>(node)->expr);
            this->EmitOp(OpCodes::RET);
            this->DecEvalStack(1);
            break;
        case NodeType::EXPRESSION:
            this->CompileCode(CastNode<Expression>(node)->expr);
            this->EmitOp(OpCodes::POP);
            this->DecEvalStack(1);
            break;
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
            this->CompileSwitch(CastNode<Switch>(node), CastNode<Switch>(node)->test == nullptr);
            break;
        case NodeType::FALLTHROUGH:
            //  Managed by CompileSwitch* method
            break;
        case NodeType::ASSIGN:
            if (CastNode<Assignment>(node)->kind == scanner::TokenType::EQUAL) {
                this->CompileAssignment(CastNode<Assignment>(node));
                break;
            }
            this->CompileCode(CastNode<Assignment>(node)->assignee);
            this->CompileCode(CastNode<Assignment>(node)->right);
            if (CastNode<Assignment>(node)->kind == scanner::TokenType::PLUS_EQ)
                this->EmitOp(OpCodes::IPADD);
            else if (CastNode<Assignment>(node)->kind == scanner::TokenType::MINUS_EQ)
                this->EmitOp(OpCodes::IPSUB);
            else if (CastNode<Assignment>(node)->kind == scanner::TokenType::ASTERISK_EQ)
                this->EmitOp(OpCodes::IPMUL);
            else // TokenType::SLASH_EQ
                this->EmitOp(OpCodes::IPDIV);
            this->DecEvalStack(1);
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
            this->DecEvalStack(1); //  POP one element and replace first!
            break;
        case NodeType::EQUALITY:
            this->CompileCode(CastNode<Binary>(node)->left);
            this->CompileCode(CastNode<Binary>(node)->right);
            if (CastNode<Binary>(node)->kind == scanner::TokenType::EQUAL_EQUAL)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::EQ);
            else if (CastNode<Binary>(node)->kind == scanner::TokenType::NOT_EQUAL)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::NE);
            this->DecEvalStack(1); //  POP one element and replace first!
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
            this->DecEvalStack(1); //  POP one element and replace first!
            break;
        case NodeType::BINARY_OP:
            this->CompileCode(CastNode<Binary>(node)->left);
            this->CompileCode(CastNode<Binary>(node)->right);
            this->CompileBinaryExpr(CastNode<Binary>(node));
            this->DecEvalStack(1); //  POP one element and replace first!
            break;
        case NodeType::UNARY_OP:
            this->CompileCode(CastNode<Unary>(node)->expr);
            this->CompileUnaryExpr(CastNode<Unary>(node));
            break;
        case NodeType::UPDATE:
            /*
            this->CompileCode(CastNode<Update>(node)->expr);
            if (CastNode<Update>(node)->kind == scanner::TokenType::PLUS_PLUS)
                this->EmitOp(CastNode<Update>(node)->prefix ? OpCodes::PREI : OpCodes::PSTI);
            else // TokenType::MINUS_MINUS
                this->EmitOp(CastNode<Update>(node)->prefix ? OpCodes::PRED : OpCodes::PSTD);
            */
            break;
        case NodeType::TUPLE:
        case NodeType::LIST:
        case NodeType::SET:
        case NodeType::MAP:
            this->CompileCompound(CastNode<ast::List>(node));
            break;
        case NodeType::IDENTIFIER:
            this->LoadVariable(CastNode<Identifier>(node)->value);
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
    BasicBlock *head;

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
            this->DecEvalStack(list->expressions.size());
            return;
        case NodeType::LIST:
            this->EmitOp4(OpCodes::MK_LIST, list->expressions.size());
            this->DecEvalStack(list->expressions.size());
            return;
        case NodeType::SET:
            this->EmitOp4(OpCodes::MK_SET, list->expressions.size());
            this->DecEvalStack(list->expressions.size());
            return;
        case NodeType::MAP:
            this->EmitOp4(OpCodes::MK_MAP, list->expressions.size() / 2);
            this->DecEvalStack(list->expressions.size() / 2);
            return;
        default:
            assert(false);
    }
}

void Compiler::CompileSwitch(const ast::Switch *stmt, bool as_if) {
    BasicBlock *last = this->NewBlock();
    BasicBlock *cond = this->cu_curr_->bb_curr;
    BasicBlock *fbody = this->NewBlock();
    BasicBlock *body = fbody;
    BasicBlock *def = nullptr;
    unsigned short index = 1;

    if (!as_if)
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
                if (!as_if)
                    this->EmitOp(OpCodes::TEST);
                this->EmitOp4(OpCodes::JT, 0);
                cond->flow_else = body;
            }
        } else {
            // Default
            def = this->NewBlock();
            this->cu_curr_->bb_curr = def;
            if (!as_if)
                this->EmitOp(OpCodes::POP);
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
        if (!as_if)
            this->EmitOp(OpCodes::POP);
        this->EmitOp4(OpCodes::JMP, 0);
        this->cu_curr_->bb_curr->flow_else = last;
    } else this->UseAsNextBlock(def);

    this->cu_curr_->bb_curr->flow_next = fbody;

    this->cu_curr_->bb_curr = body;
    this->UseAsNextBlock(last);
}

void Compiler::NewVariable(const std::string &name) {
    Symbol *sym = this->cu_curr_->symt->Insert(name);
    argon::object::List *dest = this->cu_curr_->names;

    ArObject *arname;
    bool known = false;

    if (sym == nullptr) {
        // Variable already exists
        sym = this->cu_curr_->symt->Lookup(name);
        if (sym->declared)
            throw RedeclarationException("redeclaration of variable: " + name);
        known = true;
    }

    sym->declared = true;

    if (this->cu_curr_->scope == CUScope::MODULE) {
        this->EmitOp2(OpCodes::NGV, known ? sym->id : dest->len);
        if (known)
            return;
    } else {
        dest = this->cu_curr_->locals;
        this->EmitOp2(OpCodes::NLV, dest->len);
    }

    if (known)
        arname = ListGetItem(!sym->free ? this->cu_curr_->names : this->cu_curr_->deref, sym->id);
    else {
        // Convert name string to ArObject
        if ((arname = StringNew(name)) == nullptr)
            throw MemoryException("NewVariable: StringNew");
    }

    sym->id = dest->len;
    bool ok = ListAppend(dest, arname);
    Release(arname);
    if (!ok)
        throw MemoryException("NewVariable: ListAppend");
}

void Compiler::CompileVariable(const ast::Variable *variable) {
    this->CompileCode(variable->value);
    this->NewVariable(variable->name);
    this->DecEvalStack(1);
}

void Compiler::LoadVariable(const std::string &name) {
    Symbol *sym = this->cu_curr_->symt->Lookup(name);
    argon::object::List *dest = this->cu_curr_->names;
    ArObject *tmp;

    this->IncEvalStack();

    if (sym == nullptr) {
        sym = this->cu_curr_->symt->Insert(name);

        if ((tmp = StringNew(name)) == nullptr)
            throw MemoryException("LoadVariable: StringNew");

        // Check if identifier is a Free Variable
        for (CompileUnit *cu = this->cu_curr_; cu != nullptr && cu->scope == CUScope::FUNCTION; cu = cu->prev) {
            Symbol *psym = cu->symt->Lookup(name);
            if (psym != nullptr && (psym->declared || psym->free)) {
                dest = this->cu_curr_->deref;
                sym->free = true;
            }
        }

        sym->id = dest->len;
        bool ok = ListAppend(dest, tmp);
        Release(tmp);

        if (!ok)
            throw MemoryException("LoadVariable: ListAppend");
    }

    if (this->cu_curr_->scope == CUScope::FUNCTION) {
        if (sym->declared) {
            // Local variable
            this->EmitOp2(OpCodes::LDLC, sym->id);
            return;
        }

        if (sym->free) {
            // Closure variable
            this->EmitOp2(OpCodes::LDENC, sym->id);
            return;
        }
    }

    this->EmitOp2(OpCodes::LDGBL, sym->id);
}

void Compiler::CompileAssignment(const ast::Assignment *assign) {
    // TODO: STUB
    if (assign->assignee->type == NodeType::IDENTIFIER) {
        this->CompileCode(assign->right);
        auto identifier = CastNode<Identifier>(assign->assignee);
        Symbol *sym = this->cu_curr_->symt->Lookup(identifier->value);

        this->DecEvalStack(1);

        if (sym == nullptr) {
            sym = this->cu_curr_->symt->Insert(identifier->value);
            sym->id = this->cu_curr_->names->len;

            auto id = StringNew(identifier->value);
            assert(id != nullptr);
            ListAppend(this->cu_curr_->names, id);
            Release(id);

            this->EmitOp2(OpCodes::STGBL, sym->id);
            return;
        }

        if (this->cu_curr_->symt->level == sym->table->level && this->cu_curr_->scope == CUScope::FUNCTION) {
            this->EmitOp2(OpCodes::STLC, sym->id);
            return;
        }

        this->EmitOp2(OpCodes::STGBL, sym->id);
    } else {
        assert(false);
    }
}

void Compiler::CompileLiteral(const ast::Literal *literal) {
    ArObject *obj;
    ArObject *tmp;
    unsigned short idx;

    switch (literal->kind) {
        case scanner::TokenType::NIL:
            obj = NilVal;
            IncRef(obj);
            break;
        case scanner::TokenType::FALSE:
            obj = False;
            IncRef(obj);
            break;
        case scanner::TokenType::TRUE:
            obj = True;
            IncRef(obj);
            break;
        case scanner::TokenType::STRING:
            obj = StringNew(literal->value);
            break;
        case scanner::TokenType::NUMBER_BIN:
            obj = IntegerNewFromString(literal->value, 2);
            break;
        case scanner::TokenType::NUMBER_OCT:
            obj = IntegerNewFromString(literal->value, 8);
            break;
        case scanner::TokenType::NUMBER:
            obj = IntegerNewFromString(literal->value, 10);
            break;
        case scanner::TokenType::NUMBER_HEX:
            obj = IntegerNewFromString(literal->value, 16);
            break;
        case scanner::TokenType::DECIMAL:
            obj = DecimalNewFromString(literal->value);
            break;
        default:
            assert(false);
    }

    // Check obj != nullptr
    assert(obj != nullptr);

    tmp = MapGet(this->cu_curr_->statics_map, obj);

    if (tmp == nullptr) {
        tmp = MapGet(this->statics_global, obj);
        if (tmp != nullptr) {
            Release(obj);
            obj = tmp;
            IncRef(obj);
        } else
            ListAppend(this->cu_curr_->statics, obj);
        idx = this->cu_curr_->statics->len - 1;

        auto inx_obj = IntegerNew(idx);
        assert(inx_obj != nullptr);
        MapInsert(this->cu_curr_->statics_map, obj, inx_obj);
        Release(inx_obj);

        MapInsert(this->statics_global, obj, obj);

    } else idx = (((Integer *) tmp))->integer;

    this->EmitOp2(OpCodes::LSTATIC, idx);
    this->IncEvalStack();
    Release(obj);
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
    try {
        auto bb = new BasicBlock();
        bb->link_next = this->cu_curr_->bb_list;
        this->cu_curr_->bb_list = bb;
        if (this->cu_curr_->bb_start == nullptr)
            this->cu_curr_->bb_start = bb;
        return bb;
    } catch (std::bad_alloc &) {
        throw MemoryException("Compiler: NewBlock");
    }
}

BasicBlock *Compiler::NewNextBlock() {
    BasicBlock *next = this->NewBlock();
    BasicBlock *prev = this->cu_curr_->bb_curr;

    if (this->cu_curr_->bb_curr != nullptr)
        this->cu_curr_->bb_curr->flow_next = next;
    this->cu_curr_->bb_curr = next;

    return prev;
}

Code *Compiler::Compile(std::istream *source) {
    Parser parser(source);
    std::unique_ptr<ast::Program> program = parser.Parse();

    this->EnterScope(program->filename, CUScope::MODULE);

    this->NewNextBlock();

    for (auto &stmt : program->body)
        this->CompileCode(stmt);

    this->Dfs(this->cu_curr_, this->cu_curr_->bb_start); // TODO stub DFS

    return this->Assemble();
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

Code *Compiler::Assemble() {
    auto buffer = (unsigned char *) argon::memory::Alloc(this->cu_curr_->instr_sz);
    size_t offset = 0;

    assert(this->cu_curr_->stack_cu_sz == 0);
    assert(buffer != nullptr);

    for (auto &bb : this->cu_curr_->bb_splist) {
        // Calculate JMP offset
        if (bb->flow_else != nullptr) {
            auto j_off = (OpCodes) (*((Instr32 *) (bb->instrs + (bb->instr_sz - sizeof(Instr32)))) & (Instr8) 0xFF);
            auto jmp = (Instr32) bb->flow_else->start << (unsigned char) 8 | (Instr8) j_off;
            *((Instr32 *) (bb->instrs + (bb->instr_sz - sizeof(Instr32)))) = jmp;
        }
        // Copy instrs to destination CodeObject
        argon::memory::MemoryCopy(buffer + offset, bb->instrs, bb->instr_sz);
        offset += bb->instr_sz;
    }

    return argon::object::CodeNew(buffer, this->cu_curr_->instr_sz, this->cu_curr_->stack_sz, this->cu_curr_->statics,
                                  this->cu_curr_->names, this->cu_curr_->locals, this->cu_curr_->deref);
}

void Compiler::IncEvalStack() {
    this->cu_curr_->stack_cu_sz++;
    if (this->cu_curr_->stack_cu_sz > this->cu_curr_->stack_sz)
        this->cu_curr_->stack_sz = this->cu_curr_->stack_cu_sz;
}

void Compiler::DecEvalStack(int value) {
    this->cu_curr_->stack_cu_sz -= value;
    assert(this->cu_curr_->stack_cu_sz < 0x00FFFFFF);
}

Compiler::Compiler() {
    this->statics_global = MapNew();
    assert(this->statics_global != nullptr);
}

Compiler::~Compiler() {
    Release(this->statics_global);
}
