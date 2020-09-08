// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>
#include <object/datatype/string.h>
#include <object/datatype/integer.h>
#include <object/datatype/decimal.h>
#include <object/datatype/nil.h>
#include <object/datatype/bool.h>
#include <object/datatype/code.h>
#include <object/datatype/function.h>
#include <object/datatype/namespace.h>

#include "compiler.h"
#include "parser.h"

using namespace lang;
using namespace lang::ast;
using namespace lang::symbol_table;
using namespace argon::object;

inline unsigned char AttrToFlags(bool pub, bool constant, bool weak, bool member) {
    unsigned char flags = 0;
    if (pub)
        flags |= ARGON_OBJECT_NS_PROP_PUB;
    if (constant)
        flags |= ARGON_OBJECT_NS_PROP_CONST;
    if (weak)
        flags |= ARGON_OBJECT_NS_PROP_WEAK;
    if (member)
        flags |= ARGON_OBJECT_NS_PROP_MEMBER;
    return flags;
}

Compiler::Compiler() {
    this->statics_global = MapNew();
    assert(this->statics_global != nullptr);
}

Compiler::~Compiler() {
    Release(this->statics_global);
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

bool Compiler::PushStatic(ArObject *obj, bool store, bool emit_op, unsigned int *out_idx) {
    ArObject *tmp = nullptr;
    ArObject *index = nullptr;
    unsigned int idx = -1;
    bool ok = false;

    IncRef(obj);

    if (store) {
        // check if the object is already present
        tmp = MapGet(this->cu_curr_->statics_map, obj);

        if (tmp == nullptr) {
            // Object not found in the current compile_unit, try in global statics
            if ((tmp = MapGet(this->statics_global, obj)) != nullptr) {
                Release(obj);
                obj = tmp;
            }

            if ((index = IntegerNew(this->cu_curr_->statics->len)) == nullptr)
                goto error;

            if (!MapInsert(this->cu_curr_->statics_map, obj, index))
                goto error;

            // Add object in the global statics
            if (!MapInsert(this->statics_global, obj, obj))
                goto error;

        } else idx = ((Integer *) tmp)->integer;
    }

    if (!store || idx == -1) {
        idx = this->cu_curr_->statics->len;
        if (!ListAppend(this->cu_curr_->statics, obj))
            goto error;
    }

    ok = true;

    if (emit_op) {
        this->EmitOp2(OpCodes::LSTATIC, idx);
        this->IncEvalStack();
    }

    if (out_idx != nullptr)
        *out_idx = idx;

    error:
    Release(tmp);
    Release(index);
    Release(obj);

    return ok;
}

bool Compiler::PushStatic(const std::string &value, bool emit_op, unsigned int *out_idx) {
    ArObject *tmp;

    if ((tmp = StringNew(value)) == nullptr)
        return false;

    bool ok = this->PushStatic(tmp, true, emit_op, out_idx);
    Release(tmp);

    return ok;
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

Code *Compiler::Compile(std::istream *source) {
    Parser parser(source);
    std::unique_ptr<ast::Program> program = parser.Parse();

    this->EnterScope(program->filename, CUScope::MODULE);

    for (auto &stmt : program->body)
        this->CompileCode(stmt);

    this->Dfs(this->cu_curr_, this->cu_curr_->bb_start); // TODO stub DFS

    return this->Assemble();
}

argon::object::Code *Compiler::AssembleFunction(const ast::Function *function) {
    argon::object::Code *code;
    MkFuncFlags fn_flags = MkFuncFlags::PLAIN;
    size_t p_count = function->params.size();

    this->EnterScope(function->id, CUScope::FUNCTION);

    // Push self as first param in method definition
    if (this->cu_curr_->prev != nullptr && !function->id.empty()) {
        auto prev = this->cu_curr_->prev;
        if (prev->scope == CUScope::STRUCT || prev->scope == CUScope::TRAIT) {
            this->NewVariable("self", false, 0);
            p_count++;
        }
    }

    for (auto &param: function->params) {
        auto id = CastNode<Identifier>(param);
        this->NewVariable(id->value, false, 0);
        if (id->rest_element)
            fn_flags = MkFuncFlags::VARIADIC;
    }

    this->CompileCode(function->body);

    if (CastNode<Block>(function->body)->stmts.empty() ||
        CastNode<Block>(function->body)->stmts.back()->type != NodeType::RETURN)
        this->EmitOp(OpCodes::RET);

    this->Dfs(this->cu_curr_, this->cu_curr_->bb_start);

    code = this->Assemble();

    this->ExitScope();

    // Push to static resources
    if (!this->PushStatic(code, false, true, nullptr))
        throw MemoryException("CompileFunction: PushStatic");

    if (code->enclosed->len > 0) {
        for (int i = 0; i < code->enclosed->len; i++) {
            auto st = (String *) TupleGetItem(code->enclosed, i);
            this->LoadVariable(std::string((char *) st->buffer, st->len));
            Release(st);
        }
        this->DecEvalStack(code->enclosed->len);
        this->EmitOp4(OpCodes::MK_LIST, code->enclosed->len);
        fn_flags |= MkFuncFlags::CLOSURE;
    }

    this->EmitOp4Flags(OpCodes::MK_FUNC, (unsigned char) fn_flags, p_count);

    return code;
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

            this->EmitOp4(OpCodes::STGBL, sym->id);
            return;
        }

        if (sym->declared && (this->cu_curr_->scope == CUScope::FUNCTION || sym->nested > 0)) {
            this->EmitOp2(OpCodes::STLC, sym->id);
            return;
        }

        this->EmitOp4(OpCodes::STGBL, sym->id);
    } else if (assign->assignee->type == NodeType::SUBSCRIPT) {
        this->CompileSubscr(CastNode<Binary>(assign->assignee), assign->right);
    } else if (assign->assignee->type == NodeType::MEMBER) {
        this->CompileMember(CastNode<Member>(assign->assignee), &assign->right); // TODO: impl NULLABLE
    }
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

    this->cu_curr_->symt->EnterSubScope();
    this->CompileCode(stmt->body);
    this->cu_curr_->symt->ExitSubScope();

    if (stmt->orelse != nullptr) {
        this->cu_curr_->bb_curr->flow_else = last;
        this->EmitOp4(OpCodes::JMP, 0);
        this->NewNextBlock();
        test->flow_else = this->cu_curr_->bb_curr;
        this->cu_curr_->symt->EnterSubScope();
        this->CompileCode(stmt->orelse);
        this->cu_curr_->symt->ExitSubScope();
    }

    this->UseAsNextBlock(last);
}

void Compiler::CompileCode(const ast::NodeUptr &node) {
    BasicBlock *tmp;
    unsigned int stack_sz_tmp;

    switch (node->type) {
        case NodeType::IMPORT:
            this->CompileImport(CastNode<ast::Import>(node));
            break;
        case NodeType::IMPORT_FROM:
            this->CompileImportFrom(CastNode<ast::Import>(node));
            break;
        case NodeType::STRUCT:
        case NodeType::TRAIT:
            this->CompileConstruct(CastNode<ast::Construct>(node));
            break;
        case NodeType::STRUCT_INIT:
            this->CompileStructInit(CastNode<ast::StructInit>(node));
            break;
        case NodeType::MEMBER:
            this->CompileMember(CastNode<Member>(node), nullptr);
            break;
        case NodeType::SUBSCRIPT:
            this->CompileSubscr(CastNode<Binary>(node), nullptr);
            break;
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
        case NodeType::NULLABLE:
            tmp = this->NewBlock();
            this->cu_curr_->nullable_stack.push_back(tmp);
            this->CompileCode(CastNode<Unary>(node)->expr);
            this->cu_curr_->nullable_stack.pop_back();
            this->UseAsNextBlock(tmp);
            break;
        case NodeType::EXPRESSION:
            this->CompileCode(CastNode<Unary>(node)->expr);
            this->EmitOp(OpCodes::POP);
            this->DecEvalStack(1);
            break;
        case NodeType::CONSTANT:
            this->CompileCode(CastNode<Constant>(node)->value);
            this->NewVariable(CastNode<Constant>(node)->name, true,
                              AttrToFlags(CastNode<Constant>(node)->pub, true, false, false));
            this->DecEvalStack(1);
            break;
        case NodeType::VARIABLE:
            this->CompileVariable(CastNode<Variable>(node));
            break;
        case NodeType::BLOCK:
            for (auto &stmt : CastNode<Block>(node)->stmts)
                this->CompileCode(stmt);
            break;
        case NodeType::FOR:
            this->CompileForLoop(CastNode<ast::For>(node));
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
            this->IncEvalStack();
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
            this->DecEvalStack(list->expressions.size());
            return;
        default:
            assert(false);
    }
}

void Compiler::CompileForLoop(const ast::For *loop) {
    BasicBlock *last = this->NewBlock();
    BasicBlock *head;

    this->cu_curr_->symt->EnterSubScope();

    if (loop->init != nullptr)
        this->CompileCode(loop->init);

    this->NewNextBlock();
    head = this->cu_curr_->bb_curr;

    this->CompileCode(loop->test);
    this->EmitOp4(OpCodes::JF, 0);
    this->DecEvalStack(1);
    this->cu_curr_->bb_curr->flow_else = last;

    this->NewNextBlock();
    this->CompileCode(loop->body);

    if (loop->inc != nullptr)
        this->CompileCode(loop->inc);

    this->EmitOp4(OpCodes::JMP, 0);
    this->cu_curr_->bb_curr->flow_else = head;

    this->UseAsNextBlock(last);
    this->cu_curr_->symt->ExitSubScope();
}

void Compiler::CompileFunction(const ast::Function *function) {
    Code *code = this->AssembleFunction(function);

    if (code == nullptr)
        throw MemoryException("AssembleFunction");

    if (!function->id.empty()) {
        auto is_method = this->cu_curr_->scope == CUScope::STRUCT || this->cu_curr_->scope == CUScope::TRAIT;
        this->NewVariable(function->id, true, AttrToFlags(function->pub, true, false, is_method));
        this->DecEvalStack(1);
    }
}

void Compiler::CompileImport(const ast::Import *import) {
    std::string path;
    unsigned int idx;

    for (auto &name : import->names) {
        auto name_alias = CastNode<Alias>(name);
        std::string name_to_store;

        path = CastNode<ImportName>(name_alias->value)->name;
        name_to_store = CastNode<ImportName>(name_alias->value)->import_as;

        if (!this->PushStatic(path, false, &idx))
            throw MemoryException("CompileImport: PushStatic");

        this->EmitOp2(OpCodes::IMPMOD, idx);
        this->IncEvalStack();

        if (name_alias->name != nullptr)
            name_to_store = CastNode<Identifier>(name_alias->name)->value;

        this->NewVariable(name_to_store, true, AttrToFlags(false, true, false, false));
        this->DecEvalStack(1);
    }
}

void Compiler::CompileImportFrom(const ast::Import *import) {
    unsigned int idx;

    if (!this->PushStatic(CastNode<ImportName>(import->module)->name, false, &idx))
        throw MemoryException("CompileImport: PushStatic");

    this->EmitOp2(OpCodes::IMPMOD, idx);
    this->IncEvalStack();

    for (auto &name : import->names) {
        auto name_alias = CastNode<Alias>(name);
        std::string name_to_store;

        name_to_store = CastNode<ImportName>(name_alias->value)->name;

        if (!this->PushStatic(name_to_store, false, &idx))
            throw MemoryException("CompileImport: PushStatic");

        this->EmitOp2(OpCodes::IMPFRM, idx);
        this->IncEvalStack();

        if (name_alias->name != nullptr)
            name_to_store = CastNode<Identifier>(name_alias->name)->value;

        this->NewVariable(name_to_store, true, AttrToFlags(false, true, false, false));
        this->DecEvalStack(1);
    }

    this->EmitOp(OpCodes::POP);
    this->DecEvalStack(1);
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
        }

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

void Compiler::CompileLoop(const ast::Loop *loop) {
    BasicBlock *last = this->NewBlock();
    BasicBlock *head;

    this->NewNextBlock();
    head = this->cu_curr_->bb_curr;
    if (loop->test != nullptr) {
        this->CompileCode(loop->test);
        this->EmitOp4(OpCodes::JF, 0);
        this->DecEvalStack(1);
        this->cu_curr_->bb_curr->flow_else = last;
        this->NewNextBlock();
    }

    this->cu_curr_->symt->EnterSubScope();
    this->CompileCode(loop->body);
    this->cu_curr_->symt->ExitSubScope();

    this->EmitOp4(OpCodes::JMP, 0);
    this->cu_curr_->bb_curr->flow_else = head;
    this->UseAsNextBlock(last);
}

void Compiler::CompileMember(const ast::Member *member, const ast::NodeUptr *node) {
    bool safe;
    unsigned int index;

    this->CompileCode(member->left);

    while (member->right->type == NodeType::MEMBER) {
        safe = member->safe;
        member = CastNode<Member>(member->right);

        if (!this->PushStatic(CastNode<Identifier>(member->left)->value, false, &index))
            throw MemoryException("CompileMember: PushStatic");

        if (safe) {
            this->EmitOp4(OpCodes::JNIL, 0);
            this->cu_curr_->bb_curr->flow_else = this->cu_curr_->nullable_stack.back();
            this->NewNextBlock();
        }
        this->EmitOp2(OpCodes::LDATTR, index);
    }

    if (!this->PushStatic(CastNode<Identifier>(member->right)->value, false, &index))
        throw MemoryException("CompileMember: PushStatic");

    if (member->safe) {
        this->EmitOp4(OpCodes::JNIL, 0);
        this->cu_curr_->bb_curr->flow_else = this->cu_curr_->nullable_stack.back();
        this->NewNextBlock();
    }

    if (node != nullptr) {
        this->CompileCode(*node);
        this->EmitOp2(OpCodes::STATTR, index);
        this->DecEvalStack(2);
    } else
        this->EmitOp2(OpCodes::LDATTR, index);

}

void Compiler::CompileSlice(const ast::Slice *slice) {
    unsigned char len = 1;

    this->CompileCode(slice->low);

    if (slice->type == NodeType::SLICE) {
        if (slice->high != nullptr) {
            this->CompileCode(slice->high);
            len++;
        }
        if (slice->step != nullptr) {
            this->CompileCode(slice->step);
            len++;
        }
        this->EmitOp2(OpCodes::MK_BOUNDS, len);
    }

    this->DecEvalStack(len - 1);
}

void Compiler::CompileStructInit(const ast::StructInit *init) {
    this->CompileCode(init->left);

    if (init->keys) {
        bool key = true;
        for (auto &arg:init->args) {
            if (key) {
                this->PushStatic(CastNode<Identifier>(arg)->value, true, nullptr);
                key = false;
            } else {
                this->CompileCode(arg);
                key = true;
            }
        }
    } else
        for (auto &arg : init->args)
            this->CompileCode(arg);

    this->EmitOp4Flags(OpCodes::INIT,
                       init->keys ? (unsigned char) OpCodeINITFlags::DICT : (unsigned char) OpCodeINITFlags::LIST,
                       init->args.size());

    this->DecEvalStack(init->args.size());
}

void Compiler::CompileSubscr(const ast::Binary *subscr, const ast::NodeUptr &assignable) {
    this->CompileCode(subscr->left);
    this->CompileSlice(CastNode<Slice>(subscr->right));


    if (assignable != nullptr) {
        this->CompileCode(assignable);
        this->DecEvalStack(3);
        this->EmitOp(OpCodes::STSUBSCR);
        return;
    }

    this->EmitOp(OpCodes::SUBSCR);
    this->DecEvalStack(1);
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
            if (!as_if)
                this->IncEvalStack();
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
                if (!as_if) {
                    this->EmitOp(OpCodes::TEST);
                    this->DecEvalStack(1);
                }
                this->EmitOp4(OpCodes::JT, 0);
                this->DecEvalStack(1);
                cond->flow_else = body;
            }
        } else {
            // Default
            def = this->NewBlock();
            this->cu_curr_->bb_curr = def;
            if (!as_if) {
                this->EmitOp(OpCodes::POP);
                this->DecEvalStack(1);
            }
            this->EmitOp4(OpCodes::JMP, 0);
            this->cu_curr_->bb_curr->flow_else = body;
        }

        this->cu_curr_->bb_curr = body; // Use bb pointed by body

        this->cu_curr_->symt->EnterSubScope();
        this->CompileCode(CastNode<Case>(swcase)->body);
        this->cu_curr_->symt->ExitSubScope();

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

void Compiler::CompileConstruct(const ast::Construct *construct) {
    bool structure = construct->type == NodeType::STRUCT;
    Code *co_construct;

    this->EnterScope(construct->name, structure ? CUScope::STRUCT : CUScope::TRAIT);

    this->CompileCode(construct->body);

    this->Dfs(this->cu_curr_, this->cu_curr_->bb_start);

    if ((co_construct = this->Assemble()) == nullptr)
        throw MemoryException("CompileConstruct");

    this->ExitScope();

    if (!this->PushStatic(co_construct, false, true, nullptr)) {
        Release(co_construct);
        throw MemoryException("CompileConstruct: PushStatic(struct/trait)");
    }

    // construct name
    bool ok = this->PushStatic(construct->name, true, nullptr);
    Release(co_construct);

    if (!ok)
        throw MemoryException("CompileConstruct: PushStatic");

    // impls
    for (auto &impl:construct->impls)
        this->CompileCode(impl);

    this->EmitOp2(structure ? OpCodes::MK_STRUCT : OpCodes::MK_TRAIT, construct->impls.size());
    this->DecEvalStack(construct->impls.size() + 1);

    this->NewVariable(construct->name, true, AttrToFlags(construct->pub, false, false, false));
    this->DecEvalStack(1);
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

void Compiler::CompileVariable(const ast::Variable *variable) {
    this->CompileCode(variable->value);
    this->NewVariable(variable->name, true, AttrToFlags(variable->pub, false, variable->weak,
                                                        this->cu_curr_->scope == CUScope::STRUCT));
    this->DecEvalStack(1);
}

void Compiler::DecEvalStack(int value) {
    this->cu_curr_->stack_cu_sz -= value;
    assert(this->cu_curr_->stack_cu_sz < 0x00FFFFFF);
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

void Compiler::EmitOp4Flags(OpCodes code, unsigned char flags, unsigned short arg) {
    Instr32 istr = ((flags << (unsigned char) 24)) | ((unsigned short) (arg << (unsigned char) 8)) | (Instr8) code;
    this->cu_curr_->bb_curr->AddInstr(istr);
}

void Compiler::EnterScope(const std::string &scope_name, CUScope scope) {
    CompileUnit *cu = &this->cu_list_.emplace_back(scope);
    cu->symt = std::make_unique<SymbolTable>(scope_name);
    cu->prev = this->cu_curr_;
    this->cu_curr_ = cu;
    this->NewNextBlock();
}

void Compiler::ExitScope() {
    this->cu_curr_ = this->cu_curr_->prev;
    this->cu_list_.pop_back();
}

void Compiler::IncEvalStack() {
    this->cu_curr_->stack_cu_sz++;
    if (this->cu_curr_->stack_cu_sz > this->cu_curr_->stack_sz)
        this->cu_curr_->stack_sz = this->cu_curr_->stack_cu_sz;
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

    if (this->cu_curr_->scope == CUScope::FUNCTION || sym->nested > 0) {
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

    this->EmitOp4(OpCodes::LDGBL, sym->id);
}

void Compiler::NewVariable(const std::string &name, bool emit_op, unsigned char flags) {
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

    if (this->cu_curr_->scope != CUScope::FUNCTION && sym->nested == 0) {
        if (emit_op)
            this->EmitOp4Flags(OpCodes::NGV, flags, known ? sym->id : dest->len);
        if (known)
            return;
    } else {
        dest = this->cu_curr_->locals;
        if (emit_op)
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

void Compiler::UseAsNextBlock(BasicBlock *block) {
    this->cu_curr_->bb_curr->flow_next = block;
    this->cu_curr_->bb_curr = block;
}
