// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include <object/datatype/nil.h>
#include <object/datatype/bool.h>
#include <object/datatype/bytes.h>
#include <object/datatype/integer.h>
#include <object/datatype/function.h>
#include <object/datatype/decimal.h>
#include <object/datatype/namespace.h>
#include <object/datatype/string.h>

#include "compiler_exception.h"
#include "lang/parser.h"
#include "compiler.h"

using namespace argon::memory;
using namespace argon::lang;
using namespace argon::object;

argon::object::Code *Compiler::Assemble() {
    unsigned char *buffer = nullptr;
    size_t offset = 0;

    this->unit_->Dfs();

    if (this->unit_->instr_sz > 0) {
        if ((buffer = (unsigned char *) Alloc(this->unit_->instr_sz)) == nullptr)
            throw std::bad_alloc();
    }

    for (BasicBlock *cu = this->unit_->bb.flow_head; cu != nullptr; cu = cu->block_next) {
        // Calculate JMP offset
        if (cu->flow.jump != nullptr) {
            auto j_off = (OpCodes) (*((Instr32 *) (cu->instr + (cu->instr_sz - sizeof(Instr32)))) & (Instr8) 0xFF);
            auto jmp = (Instr32) cu->flow.jump->instr_sz_start << (unsigned char) 8 | (Instr8) j_off;
            *((Instr32 *) (cu->instr + (cu->instr_sz - sizeof(Instr32)))) = jmp;
        }
        // Copy instrs to destination CodeObject
        MemoryCopy(buffer + offset, cu->instr, cu->instr_sz);
        offset += cu->instr_sz;
    }

    return CodeNew(buffer, this->unit_->instr_sz, this->unit_->stack.required, this->unit_->statics, this->unit_->names,
                   this->unit_->locals, this->unit_->enclosed);
}

argon::object::Code *Compiler::CompileFunction(const ast::Function *func) {
    FunctionType fun_flags{};
    unsigned short p_count = func->params.size();
    Code *fu_code;

    this->EnterContext(func->id, TUScope::FUNCTION);

    // Push self as first param in method definition
    if (!func->id.empty()) {
        if (this->unit_->prev->scope == TUScope::STRUCT || this->unit_->prev->scope == TUScope::TRAIT) {
            this->VariableNew("self", false, 0);
            fun_flags |= FunctionType::METHOD;
            p_count++;
        }
    }

    // Store params name
    for (auto &param : func->params) {
        auto id = ast::CastNode<ast::Identifier>(param);
        this->VariableNew(id->value, false, 0);
        if (id->rest_element) {
            fun_flags |= FunctionType::VARIADIC;
            p_count--;
        }
    }

    this->CompileCode(func->body);

    {
        // If the function is empty or the last statement is not return,
        // forcefully enter return as last statement
        auto block = ast::CastNode<ast::Block>(func->body);
        if (block->stmts.empty() || block->stmts.back()->type != ast::NodeType::RETURN)
            this->EmitOp(OpCodes::RET);
    }

    fu_code = this->Assemble();

    this->ExitContext();

    if (this->PushStatic(!func->id.empty() ? func->id : "<anonymous>", true, true) == -1)
        throw std::bad_alloc();

    if (this->PushStatic(fu_code, false, true) == -1) {
        throw std::bad_alloc();
    }

    if (fu_code->enclosed->len > 0) {
        for (int i = 0; i < fu_code->enclosed->len; i++) {
            auto st = (String *) fu_code->enclosed->objects[i];
            this->VariableLoad(std::string((char *) st->buffer, st->len));
        }
        this->unit_->DecStack(fu_code->enclosed->len);
        this->EmitOp4(OpCodes::MK_LIST, fu_code->enclosed->len);
        fun_flags |= FunctionType::CLOSURE;
    }

    this->EmitOp4Flags(OpCodes::MK_FUNC, (unsigned char) fun_flags, p_count);

    return fu_code;
}

bool Compiler::IsFreeVariable(const std::string &name) {
    // Look back in the TranslationUnits,
    // if a variable with the same name exists and is declared or free
    // in turn then this is a free variable
    for (TranslationUnit *tu = this->unit_; tu != nullptr && tu->scope == TUScope::FUNCTION; tu = tu->prev) {
        auto sym = tu->symt.Lookup(name);
        if (sym != nullptr && (sym->declared || sym->free))
            return true;
    }

    return false;
}

bool Compiler::VariableLookupCreate(const std::string &name, Symbol **out_sym) {
    Symbol *sym = this->unit_->symt.Lookup(name);
    bool created = false;

    if (sym == nullptr) {
        List *dst = this->unit_->names;
        String *tmp;

        sym = this->unit_->symt.Insert(name);

        if ((tmp = StringNew(name)) == nullptr)
            throw std::bad_alloc();

        // Check if identifier is a Free Variable (Closure)
        if (this->IsFreeVariable(name)) {
            sym->type = SymbolType::VARIABLE;
            dst = this->unit_->enclosed;
            sym->free = true;
        }

        sym->id = dst->len;
        bool ok = ListAppend(dst, tmp);
        Release(tmp);
        if (!ok)
            throw std::bad_alloc();

        created = true;
    }

    if (out_sym != nullptr)
        (*out_sym) = sym;

    return created;
}

inline unsigned char AttrToFlags(bool pub, bool constant, bool weak, bool member) {
    PropertyType flags{};

    if (pub)
        flags |= PropertyType::PUBLIC;
    if (constant)
        flags |= PropertyType::CONST;
    if (weak)
        flags |= PropertyType::WEAK;
    if (!member)
        flags |= PropertyType::STATIC;

    return (unsigned char) flags;
}

unsigned int Compiler::CompileMember(const ast::Member *member, bool duplicate, bool emit_last) {
    unsigned int index;

    this->CompileCode(member->left);

    while (member->right->type == ast::NodeType::MEMBER) {
        bool safe = member->safe;
        member = ast::CastNode<ast::Member>(member->right);

        index = this->PushStatic(ast::CastNode<ast::Identifier>(member->left)->value, true, false);

        if (safe) {
            this->CompileJump(OpCodes::JNIL, this->unit_->bb.stack);
            this->unit_->BlockAsNextNew();
        }

        this->EmitOp4(OpCodes::LDATTR, index);
    }

    // Ok now store last member
    if (member->right->type != ast::NodeType::IDENTIFIER)
        throw InvalidSyntaxtException("expected identifier");
    index = this->PushStatic(ast::CastNode<ast::Identifier>(member->right)->value, true, false);

    if (member->safe) {
        this->CompileJump(OpCodes::JNIL, this->unit_->bb.stack);
        this->unit_->BlockAsNextNew();
    }

    if (duplicate) {
        this->EmitOp2(OpCodes::DUP, 1);
        this->unit_->IncStack();
    }

    if (emit_last)
        this->EmitOp4(OpCodes::LDATTR, index);

    return index;
}

unsigned int Compiler::CompileScope(const ast::Scope *scope, bool duplicate, bool emit_last) {
    unsigned int id = 0;
    size_t idx = 0;

    for (auto &seg : scope->segments) {
        if (idx++ == 0) {
            this->VariableLoad(seg);
            continue;
        }

        id = this->PushStatic(seg, true, false);

        if (idx < scope->segments.size())
            this->EmitOp4(OpCodes::LDSCOPE, id);
    }

    if (duplicate) {
        this->EmitOp2(OpCodes::DUP, 1);
        this->unit_->IncStack();
    }

    if (emit_last)
        this->EmitOp4(OpCodes::LDSCOPE, id);

    return id;
}

unsigned int Compiler::PushStatic(const std::string &value, bool store, bool emit) {
    unsigned int val;
    String *tmp;

    if ((tmp = StringNew(value)) == nullptr)
        throw std::bad_alloc();

    try {
        val = this->PushStatic(tmp, store, emit);
    } catch (std::bad_alloc &) {
        Release(tmp);
        throw;
    }

    Release(tmp);

    return val;
}

unsigned int Compiler::PushStatic(ArObject *obj, bool store, bool emit) {
    ArObject *tmp = nullptr;
    IntegerUnderlayer idx = -1;

    IncRef(obj);

    if (store) {
        // check if object is already present in TranslationUnit
        tmp = MapGet(this->unit_->statics_map, obj);

        if (tmp == nullptr) {
            // Object not found in the current TranslationUnit, try in global_statics
            if ((tmp = MapGet(this->statics_globals_, obj)) != nullptr) {
                // recover already existing object and discard the actual one
                Release(obj);
                obj = tmp;
            }

            ArObject *index;

            if ((index = IntegerNew(this->unit_->statics->len)) == nullptr)
                goto error;

            // Add to local map
            if (!MapInsert(this->unit_->statics_map, obj, index)) {
                Release(index);
                goto error;
            }

            Release(index);

            // Add obj to global_statics
            if (tmp == nullptr) {
                if (!MapInsert(this->statics_globals_, obj, obj))
                    goto error;
            }
        } else idx = ((Integer *) tmp)->integer;
    }

    if (!store || idx == -1) {
        idx = this->unit_->statics->len;
        if (!ListAppend(this->unit_->statics, obj))
            goto error;
    }

    if (emit) {
        this->EmitOp4(OpCodes::LSTATIC, idx);
        this->unit_->IncStack();
    }

    Release(tmp);
    Release(obj);
    return idx;

    error:
    Release(tmp);
    Release(obj);
    throw std::bad_alloc();
}

void Compiler::CompileAssignment(const ast::Assignment *assign) {
    ast::Member *member = nullptr;
    bool is_nullable = assign->assignee->type == ast::NodeType::NULLABLE;

    if (is_nullable)
        this->unit_->BlockNewEnqueue();

    if (assign->kind == scanner::TokenType::EQUAL) {
        this->CompileCode(assign->right);
        switch (assign->assignee->type) {
            case ast::NodeType::IDENTIFIER:
                this->VariableStore(ast::CastNode<ast::Identifier>(assign->assignee)->value);
                break;
            case ast::NodeType::SUBSCRIPT:
                this->CompileSubscr(ast::CastNode<ast::Binary>(assign->assignee), false, false);
                this->EmitOp(OpCodes::STSUBSCR);
                this->unit_->DecStack(3);
                break;
            case ast::NodeType::NULLABLE:
                member = ast::CastNode<ast::Member>(ast::CastNode<ast::Unary>(assign->assignee)->expr);
            case ast::NodeType::MEMBER: {
                if (member == nullptr)
                    member = ast::CastNode<ast::Member>(assign->assignee);

                auto id = this->CompileMember(member, false, false);
                this->EmitOp4(OpCodes::STATTR, id);
                this->unit_->DecStack(2);
                break;
            }
            case ast::NodeType::SCOPE: {
                auto id = this->CompileScope(ast::CastNode<ast::Scope>(assign->assignee), false, false);
                this->EmitOp4(OpCodes::STSCOPE, id);
                this->unit_->DecStack(2);
                break;
            }
            case ast::NodeType::TUPLE: {
                auto tuple = ast::CastNode<ast::List>(assign->assignee);

                this->EmitOp4(OpCodes::UNPACK, tuple->expressions.size());
                this->unit_->DecStack();

                this->unit_->IncStack(tuple->expressions.size());

                for (auto &expr : tuple->expressions) {
                    if (expr->type != ast::NodeType::IDENTIFIER)
                        throw InvalidSyntaxtException(
                                "in unpacking expression, only identifiers must be present on the left");
                    this->VariableStore(ast::CastNode<ast::Identifier>(expr)->value);
                }
                break;
            }
            default:
                assert(false);
        }
    } else
        this->CompileAugAssignment(assign);

    if (is_nullable) {
        auto block = this->unit_->BlockNew();

        this->CompileJump(OpCodes::JMP, block);
        this->unit_->BlockAsNextDequeue();

        // Remove element from the queue
        this->EmitOp(OpCodes::POP);
        this->EmitOp(OpCodes::POP);

        this->unit_->BlockAsNext(block);
    }
}

void Compiler::CompileAugAssignment(const ast::Assignment *assign) {
    ast::Member *member = nullptr;
    unsigned int id = 0;
    OpCodes code;

    // Select OpCode
    switch (assign->kind) {
        case scanner::TokenType::PLUS_EQ:
            code = OpCodes::IPADD;
            break;
        case scanner::TokenType::MINUS_EQ:
            code = OpCodes::IPSUB;
            break;
        case scanner::TokenType::ASTERISK_EQ:
            code = OpCodes::IPMUL;
            break;
        case scanner::TokenType::SLASH_EQ:
            code = OpCodes::IPDIV;
            break;
        default:
            assert(false);
    }

#define COMPILE_OP()                \
this->CompileCode(assign->right);   \
this->EmitOp(code);                 \
this->unit_->DecStack()

    switch (assign->assignee->type) {
        case ast::NodeType::IDENTIFIER: {
            auto var = ast::CastNode<ast::Identifier>(assign->assignee);

            this->VariableLoad(var->value);
            COMPILE_OP();
            this->VariableStore(var->value);
            break;
        }
        case ast::NodeType::SUBSCRIPT: {
            this->CompileSubscr(ast::CastNode<ast::Binary>(assign->assignee), true, true);
            COMPILE_OP();
            this->EmitOp2(OpCodes::PB_HEAD, 3);
            this->EmitOp(OpCodes::STSUBSCR);
            this->unit_->DecStack(3);
            break;
        }
        case ast::NodeType::NULLABLE:
            member = ast::CastNode<ast::Member>(ast::CastNode<ast::Unary>(assign->assignee)->expr);
        case ast::NodeType::MEMBER: {
            if (member == nullptr)
                member = ast::CastNode<ast::Member>(assign->assignee);

            id = this->CompileMember(member, true, true);
            COMPILE_OP();
            this->EmitOp2(OpCodes::PB_HEAD, 1);
            this->EmitOp4(OpCodes::STATTR, id);
            this->unit_->DecStack(2);
            break;
        }
        case ast::NodeType::SCOPE: {
            id = this->CompileScope(ast::CastNode<ast::Scope>(assign->assignee), true, true);
            COMPILE_OP();
            this->EmitOp2(OpCodes::PB_HEAD, 1);
            this->EmitOp4(OpCodes::STSCOPE, id);
            this->unit_->DecStack(2);
            break;
        }
        default:
            assert(false);
    }

#undef COMPILE_OP
}

void Compiler::CompileBinary(const ast::Binary *binary) {
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
    BasicBlock *end = this->unit_->BlockNew();
    BasicBlock *test;

    this->CompileCode(stmt->test);
    test = this->unit_->bb.current;

    this->CompileJump(OpCodes::JF, test, end);
    this->unit_->DecStack();

    this->unit_->BlockAsNextNew();

    this->unit_->symt.EnterSub();
    this->CompileCode(stmt->body);
    this->unit_->symt.ExitSub();

    if (stmt->orelse != nullptr) {
        this->CompileJump(OpCodes::JMP, this->unit_->bb.current, end);

        this->unit_->BlockAsNextNew();
        test->flow.jump = this->unit_->bb.current;
        this->unit_->symt.EnterSub();
        this->CompileCode(stmt->orelse);
        this->unit_->symt.ExitSub();
    }

    this->unit_->BlockAsNext(end);
}

void Compiler::CompileCall(const ast::Call *call, OpCodes code) {
    auto stack_sz = this->unit_->stack.current;
    unsigned short params = call->args.size();
    OpCodeCallFlags flags{};

    if (call->callee->type == ast::NodeType::MEMBER) {
        auto idx = this->CompileMember(ast::CastNode<ast::Member>(call->callee), false, false);
        this->EmitOp4(OpCodes::LDMETH, idx);
        this->unit_->IncStack();
        params++;
        flags |= OpCodeCallFlags::METHOD;
    } else
        this->CompileCode(call->callee);

    for (auto &arg : call->args) {
        if (arg->type == ast::NodeType::ELLIPSIS) {
            this->CompileCode(ast::CastNode<ast::Unary>(arg)->expr);
            flags |= OpCodeCallFlags::SPREAD;
            continue;
        }
        this->CompileCode(arg);
    }

    this->unit_->DecStack(this->unit_->stack.current - stack_sz);

    this->EmitOp4Flags(code, (unsigned char) flags, params); // CALL, DFR, SPWN

    this->unit_->IncStack();
}

void Compiler::CompileCode(const ast::NodeUptr &node) {
#define TARGET_TYPE(type)   case ast::NodeType::type:

    switch (node->type) {
        /*
        TARGET_TYPE(ALIAS)
            break;
            */
        TARGET_TYPE(ASSIGN)
            this->CompileAssignment(ast::CastNode<ast::Assignment>(node));
            break;
        TARGET_TYPE(BINARY_OP)
            this->CompileCode(ast::CastNode<ast::Binary>(node)->left);
            this->CompileCode(ast::CastNode<ast::Binary>(node)->right);
            this->CompileBinary(ast::CastNode<ast::Binary>(node));
            this->unit_->DecStack();
            break;
        TARGET_TYPE(BLOCK)
            for (auto &stmt : ast::CastNode<ast::Block>(node)->stmts)
                this->CompileCode(stmt);
            break;
        TARGET_TYPE(BREAK) {
            auto brk = ast::CastNode<ast::Unary>(node);
            auto meta = this->unit_->LBlockGet(
                    brk->expr != nullptr ?
                    ast::CastNode<ast::Identifier>(brk->expr)->value : "");
            if (meta == nullptr)
                throw InvalidSyntaxtException("use of keyword 'break' outside of switch/loop is forbidden");
            this->CompileJump(OpCodes::JMP, meta->end);
            this->unit_->BlockAsNextNew();
            break;
        }
        TARGET_TYPE(CALL)
            this->CompileCall(ast::CastNode<ast::Call>(node), OpCodes::CALL);
            break;
            /*
        TARGET_TYPE(COMMENT)
            break;
             */
        TARGET_TYPE(CONSTANT) {
            auto cnst = ast::CastNode<ast::Constant>(node);
            this->CompileCode(cnst->value);
            this->VariableNew(cnst->name, true, AttrToFlags(cnst->pub, true, false, false));
            break;
        }
        TARGET_TYPE(CONTINUE) {
            auto cnt = ast::CastNode<ast::Unary>(node);
            auto meta = this->unit_->LBlockGet(
                    cnt->expr != nullptr ?
                    ast::CastNode<ast::Identifier>(cnt->expr)->value : "");
            if (meta == nullptr || meta->begin == nullptr)
                throw InvalidSyntaxtException("use of keyword 'continue' outside of loop is forbidden");
            this->CompileJump(OpCodes::JMP, meta->begin);
            this->unit_->BlockAsNextNew();
            break;
        }
        TARGET_TYPE(DEFER) {
            auto dfr = ast::CastNode<ast::Unary>(node);
            if (dfr->expr->type != ast::NodeType::CALL)
                throw InvalidSyntaxtException("expression in defer must be function call");
            this->CompileCall(ast::CastNode<ast::Call>(dfr->expr), OpCodes::DFR);
            break;
        }
            /*
            TARGET_TYPE(ELLIPSIS)
                break;
                */
        TARGET_TYPE(ELVIS) {
            auto end = this->unit_->BlockNew();
            auto elvis = ast::CastNode<ast::Binary>(node);
            this->CompileCode(elvis->left);
            this->CompileJump(OpCodes::JTOP, end);
            this->unit_->BlockAsNextNew();
            this->CompileCode(elvis->right);
            this->unit_->BlockAsNext(end);
            break;
        }
        TARGET_TYPE(EQUALITY) {
            this->CompileCode(ast::CastNode<ast::Binary>(node)->left);
            this->CompileCode(ast::CastNode<ast::Binary>(node)->right);

            scanner::TokenType type = ast::CastNode<ast::Binary>(node)->kind;
            if (type == scanner::TokenType::EQUAL_EQUAL)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::EQ);
            else if (type == scanner::TokenType::NOT_EQUAL)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::NE);
            else
                assert(false);

            this->unit_->DecStack();
            break;
        }
        TARGET_TYPE(EXPRESSION)
            this->CompileCode(ast::CastNode<ast::Unary>(node)->expr);
            this->EmitOp(OpCodes::POP);
            this->unit_->DecStack();
            break;
        TARGET_TYPE(FOR)
            this->CompileForLoop(ast::CastNode<ast::For>(node), "");
            break;
        TARGET_TYPE(FOR_IN)
            this->CompileForInLoop(ast::CastNode<ast::For>(node), "");
            break;
        TARGET_TYPE(FUNC) {
            auto func = ast::CastNode<ast::Function>(node);
            this->CompileFunction(func);
            if (!func->id.empty()) {
                auto is_method = this->unit_->scope == TUScope::STRUCT || this->unit_->scope == TUScope::TRAIT;
                this->VariableNew(func->id, true, AttrToFlags(func->pub, true, false, is_method));
            }
            break;
        }
            /*
            TARGET_TYPE(GOTO)
                break;
                */
        TARGET_TYPE(IDENTIFIER)
            this->VariableLoad(ast::CastNode<ast::Identifier>(node)->value);
            break;
        TARGET_TYPE(IF)
            this->CompileBranch(ast::CastNode<ast::If>(node));
            break;
        TARGET_TYPE(IMPL)
            break;
        TARGET_TYPE(IMPORT)
            this->CompileImport(ast::CastNode<ast::Import>(node));
            break;
        TARGET_TYPE(IMPORT_FROM)
            this->CompileImportFrom(ast::CastNode<ast::Import>(node));
            break;
            /*
        TARGET_TYPE(IMPORT_NAME)
            break;
        TARGET_TYPE(INDEX)
            break;
             */
        TARGET_TYPE(LABEL) {
            auto label = ast::CastNode<ast::Binary>(node);
            if (label->right->type == ast::NodeType::LOOP)
                this->CompileLoop(ast::CastNode<ast::Loop>(label->right),
                                  ast::CastNode<ast::Identifier>(label->left)->value);
            else if (label->right->type == ast::NodeType::FOR_IN)
                this->CompileForInLoop(ast::CastNode<ast::For>(label->right),
                                       ast::CastNode<ast::Identifier>(label->left)->value);
            else if (label->right->type == ast::NodeType::FOR)
                this->CompileForLoop(ast::CastNode<ast::For>(label->right),
                                     ast::CastNode<ast::Identifier>(label->left)->value);
            else if (label->right->type == ast::NodeType::SWITCH)
                this->CompileSwitch(ast::CastNode<ast::Switch>(label->right),
                                    ast::CastNode<ast::Identifier>(label->left)->value);
            else
                this->CompileCode(label->right);
            break;
        }
        TARGET_TYPE(LIST)
        TARGET_TYPE(MAP)
        TARGET_TYPE(SET)
        TARGET_TYPE(TUPLE)
            this->CompileCompound(ast::CastNode<ast::List>(node));
            break;
        TARGET_TYPE(LITERAL)
            this->CompileLiteral(ast::CastNode<ast::Literal>(node));
            break;
        TARGET_TYPE(LOGICAL) {
            this->CompileCode(ast::CastNode<ast::Binary>(node)->left);
            this->CompileCode(ast::CastNode<ast::Binary>(node)->right);

            scanner::TokenType type = ast::CastNode<ast::Binary>(node)->kind;
            if (type == scanner::TokenType::PIPE)
                this->EmitOp(OpCodes::LOR);
            else if (type == scanner::TokenType::CARET)
                this->EmitOp(OpCodes::LXOR);
            else if (type == scanner::TokenType::AMPERSAND)
                this->EmitOp(OpCodes::LAND);
            else
                assert(false);

            this->unit_->DecStack();
            break;
        }
        TARGET_TYPE(LOOP)
            this->CompileLoop(ast::CastNode<ast::Loop>(node), "");
            break;
        TARGET_TYPE(MEMBER)
            this->CompileMember(ast::CastNode<ast::Member>(node), false, true);
            break;
        TARGET_TYPE(NULLABLE)
            this->unit_->BlockNewEnqueue();
            this->CompileCode(ast::CastNode<ast::Unary>(node)->expr);
            this->unit_->BlockAsNextDequeue();
            break;
        TARGET_TYPE(RELATIONAL) {
            this->CompileCode(ast::CastNode<ast::Binary>(node)->left);
            this->CompileCode(ast::CastNode<ast::Binary>(node)->right);

            scanner::TokenType type = ast::CastNode<ast::Binary>(node)->kind;
            if (type == scanner::TokenType::GREATER)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::GE);
            else if (type == scanner::TokenType::GREATER_EQ)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::GEQ);
            else if (type == scanner::TokenType::LESS)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::LE);
            else if (type == scanner::TokenType::LESS_EQ)
                this->EmitOp2(OpCodes::CMP, (unsigned char) CompareMode::LEQ);
            else
                assert(false);

            this->unit_->DecStack();
            break;
        }
        TARGET_TYPE(RETURN)
            this->CompileCode(ast::CastNode<ast::Unary>(node)->expr);
            this->EmitOp(OpCodes::RET);
            this->unit_->DecStack();
            break;
        TARGET_TYPE(SCOPE)
            this->CompileScope(ast::CastNode<ast::Scope>(node), false, true);
            break;
        TARGET_TYPE(SPAWN) {
            auto spawn = ast::CastNode<ast::Unary>(node);
            if (spawn->expr->type != ast::NodeType::CALL)
                throw InvalidSyntaxtException("expression in spawn must be function call");
            this->CompileCall(ast::CastNode<ast::Call>(spawn->expr), OpCodes::SPWN);
            break;
        }
        TARGET_TYPE(STRUCT)
        TARGET_TYPE(TRAIT)
            this->CompileConstruct(ast::CastNode<ast::Construct>(node));
            break;
        TARGET_TYPE(STRUCT_INIT)
            this->CompileStructInit(ast::CastNode<ast::StructInit>(node));
            break;
        TARGET_TYPE(SUBSCRIPT)
            this->CompileSubscr(ast::CastNode<ast::Binary>(node), false, true);
            break;
        TARGET_TYPE(SWITCH)
            this->CompileSwitch(ast::CastNode<ast::Switch>(node), "");
            break;
        TARGET_TYPE(TEST)
            this->CompileTest(ast::CastNode<ast::Binary>(node));
            break;
        TARGET_TYPE(UNARY_OP) {
            auto unary = ast::CastNode<ast::Unary>(node);

            this->CompileCode(unary->expr);

            switch (unary->kind) {
                case scanner::TokenType::EXCLAMATION:
                    this->EmitOp(OpCodes::NOT);
                    break;
                case scanner::TokenType::TILDE:
                    this->EmitOp(OpCodes::INV);
                    break;
                case scanner::TokenType::PLUS:
                    this->EmitOp(OpCodes::POS);
                    break;
                case scanner::TokenType::MINUS:
                    this->EmitOp(OpCodes::NEG);
                    break;
                default:
                    assert(false);
            }

            break;
        }
        TARGET_TYPE(UPDATE)
            this->CompileUpdate(ast::CastNode<ast::Update>(node));
            break;
        TARGET_TYPE(VARIABLE) {
            auto variable = ast::CastNode<ast::Variable>(node);

            if (variable->value == nullptr) {
                IncRef(NilVal);
                this->PushStatic(NilVal, true, true);
            } else this->CompileCode(variable->value);

            this->VariableNew(variable->name, true, AttrToFlags(variable->pub,
                                                                false,
                                                                variable->weak,
                                                                this->unit_->scope == TUScope::STRUCT));
            break;
        }
        default:
            assert(false);
    }

#undef TARGET_TYPE
}

void Compiler::CompileCompound(const ast::List *list) {
    for (auto &expr : list->expressions)
        this->CompileCode(expr);

    switch (list->type) {
        case ast::NodeType::TUPLE:
            this->EmitOp4(OpCodes::MK_TUPLE, list->expressions.size());
            this->unit_->DecStack(list->expressions.size());
            break;
        case ast::NodeType::LIST:
            this->EmitOp4(OpCodes::MK_LIST, list->expressions.size());
            this->unit_->DecStack(list->expressions.size());
            break;
        case ast::NodeType::SET:
            this->EmitOp4(OpCodes::MK_SET, list->expressions.size());
            this->unit_->DecStack(list->expressions.size());
            break;
        case ast::NodeType::MAP:
            this->EmitOp4(OpCodes::MK_MAP, list->expressions.size() / 2);
            this->unit_->DecStack(list->expressions.size());
            break;
        default:
            assert(false);
    }

    this->unit_->IncStack();
}

void Compiler::CompileConstruct(const ast::Construct *construct) {
    bool is_struct = construct->type == ast::NodeType::STRUCT;
    Code *code;

    this->EnterContext(construct->name, is_struct ? TUScope::STRUCT : TUScope::TRAIT);

    this->CompileCode(construct->body);

    code = this->Assemble();

    this->ExitContext();

    try {
        this->PushStatic(code, false, true);
    } catch (...) {
        Release(code);
        throw;
    }

    Release(code);
    this->PushStatic(construct->name, true, true);

    // Impls
    for (auto &impl:construct->impls)
        this->CompileCode(impl);

    this->EmitOp2(is_struct ? OpCodes::MK_STRUCT : OpCodes::MK_TRAIT, construct->impls.size());
    this->unit_->DecStack(construct->impls.size() + 1); // +1 is name

    this->VariableNew(construct->name, true, AttrToFlags(construct->pub, false, false, false));
}

void Compiler::CompileForLoop(const ast::For *loop, const std::string &name) {
    LBlockMeta *meta;

    this->unit_->symt.EnterSub();

    if (loop->init != nullptr)
        this->CompileCode(loop->init);

    meta = this->unit_->LBlockBegin(name, true);

    this->CompileCode(loop->test);
    this->unit_->DecStack();
    this->CompileJump(OpCodes::JF, meta->end);

    this->unit_->BlockAsNextNew();
    this->CompileCode(loop->body);

    if (loop->inc != nullptr)
        this->CompileCode(loop->inc);

    this->CompileJump(OpCodes::JMP, meta->begin);

    this->unit_->LBlockEnd();

    this->unit_->symt.ExitSub();
}

void Compiler::CompileForInLoop(const ast::For *loop, const std::string &name) {
    LBlockMeta *meta;

    this->unit_->symt.EnterSub();

    this->CompileCode(loop->test);

    this->EmitOp(OpCodes::LDITER);

    meta = this->unit_->LBlockBegin(name, true);

    this->CompileJump(OpCodes::NJE, meta->end);
    this->unit_->BlockAsNextNew();

    this->unit_->IncStack();

    // ASSIGN

    if (loop->init->type == ast::NodeType::IDENTIFIER)
        this->VariableStore(ast::CastNode<ast::Identifier>(loop->init)->value);
    else if (loop->init->type == ast::NodeType::TUPLE) {
        auto tuple = ast::CastNode<ast::List>(loop->init);

        this->EmitOp4(OpCodes::UNPACK, tuple->expressions.size());
        this->unit_->DecStack();

        this->unit_->IncStack(tuple->expressions.size());

        for (auto &expr : tuple->expressions) {
            if (expr->type != ast::NodeType::IDENTIFIER)
                throw InvalidSyntaxtException(
                        "in unpacking expression, only identifiers must be present on the left");
            this->VariableStore(ast::CastNode<ast::Identifier>(expr)->value);
        }
    }

    // EOL

    this->CompileCode(loop->body);

    this->CompileJump(OpCodes::JMP, meta->begin);

    this->unit_->LBlockEnd();

    this->unit_->DecStack();

    this->unit_->symt.ExitSub();
}

void Compiler::CompileImport(const ast::Import *import) {
    ast::ImportName *path;
    unsigned int idx;

    for (auto &name : import->names) {
        auto alias = ast::CastNode<ast::Alias>(name);
        std::string *name_to_store;

        path = ast::CastNode<ast::ImportName>(alias->value);
        name_to_store = &(ast::CastNode<ast::ImportName>(alias->value)->import_as);

        idx = this->PushStatic(path->name, true, false);

        this->EmitOp4(OpCodes::IMPMOD, idx);
        this->unit_->IncStack();

        if (alias->name != nullptr)
            name_to_store = &(ast::CastNode<ast::Identifier>(alias->name)->value);

        this->VariableNew(*name_to_store, true, AttrToFlags(false, true, false, false));
    }
}

void Compiler::CompileImportFrom(const ast::Import *import) {
    unsigned int idx;

    idx = this->PushStatic(ast::CastNode<ast::ImportName>(import->module)->name, true, false);

    this->EmitOp4(OpCodes::IMPMOD, idx);
    this->unit_->IncStack();


    for (auto &name : import->names) {
        auto alias = ast::CastNode<ast::Alias>(name);
        std::string *name_to_store;

        name_to_store = &(ast::CastNode<ast::ImportName>(alias->value)->name);

        idx = this->PushStatic(*name_to_store, true, false);
        this->unit_->IncStack();

        this->EmitOp4(OpCodes::IMPFRM, idx);

        if (alias->name != nullptr)
            name_to_store = &(ast::CastNode<ast::Identifier>(alias->name)->value);

        this->VariableNew(*name_to_store, true, AttrToFlags(false, true, false, false));
    }

    this->EmitOp(OpCodes::POP);
    this->unit_->DecStack();
}

void Compiler::CompileJump(OpCodes op, BasicBlock *src, BasicBlock *dest) {
    this->EmitOp4(op, 0);

    if (src != nullptr && dest != nullptr)
        src->flow.jump = dest;
}

void Compiler::CompileJump(OpCodes op, BasicBlock *dest) {
    this->EmitOp4(op, 0);
    this->unit_->bb.current->flow.jump = dest;
}

void Compiler::CompileLiteral(const ast::Literal *literal) {
    ArObject *obj = nullptr;

    switch (literal->kind) {
        case scanner::TokenType::STRING:
            obj = StringNew(literal->value);
            break;
        case scanner::TokenType::BYTE_STRING:
            obj = BytesNew(literal->value);
            break;
        case scanner::TokenType::RAW_STRING:
            obj = StringNew(literal->value);
            break;
        case scanner::TokenType::DECIMAL:
            obj = DecimalNewFromString(literal->value);
            break;
        case scanner::TokenType::NUMBER:
            obj = IntegerNewFromString(literal->value, 10);
            break;
        case scanner::TokenType::NUMBER_BIN:
            obj = IntegerNewFromString(literal->value, 2);
            break;
        case scanner::TokenType::NUMBER_OCT:
            obj = IntegerNewFromString(literal->value, 8);
            break;
        case scanner::TokenType::NUMBER_HEX:
            obj = IntegerNewFromString(literal->value, 16);
            break;
        case scanner::TokenType::FALSE:
            obj = False;
            IncRef(False);
            break;
        case scanner::TokenType::TRUE:
            obj = True;
            IncRef(True);
            break;
        case scanner::TokenType::NIL:
            obj = NilVal;
            IncRef(NilVal);
            break;
        default:
            break;
    }

    // Sanity check
    if (obj == nullptr)
        throw std::bad_alloc();

    this->PushStatic(obj, true, true);

    Release(obj);
}

void Compiler::CompileLoop(const ast::Loop *loop, const std::string &name) {
    auto meta = this->unit_->LBlockBegin(name, true);

    if (loop->test != nullptr) {
        this->CompileCode(loop->test);
        this->unit_->DecStack();
        this->CompileJump(OpCodes::JF, meta->end);
        this->unit_->BlockAsNextNew();
    }

    this->unit_->symt.EnterSub();
    this->CompileCode(loop->body);
    this->unit_->symt.ExitSub();
    this->CompileJump(OpCodes::JMP, meta->begin);

    this->unit_->LBlockEnd();
}

void Compiler::CompileSlice(const ast::Slice *slice) {
    unsigned short len = 1;

    this->CompileCode(slice->low);

    if (slice->type == ast::NodeType::SLICE) {
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

    this->unit_->DecStack(len - 1);
}

void Compiler::CompileStructInit(const ast::StructInit *init) {
    OpCodeINITFlags flag = OpCodeINITFlags::LIST;

    this->CompileCode(init->left);

    if (init->keys) {
        bool key = true;
        flag = OpCodeINITFlags::DICT;
        for (auto &arg:init->args) {
            if (key) {
                this->PushStatic(ast::CastNode<ast::Identifier>(arg)->value, true, true);
                key = false;
            } else {
                this->CompileCode(arg);
                key = true;
            }
        }
    } else {
        for (auto &arg : init->args)
            this->CompileCode(arg);
    }

    this->EmitOp4Flags(OpCodes::INIT, (unsigned char) flag, init->args.size());

    this->unit_->DecStack(init->args.size());
}

void Compiler::CompileSubscr(const ast::Binary *subscr, bool duplicate, bool emit_subscr) {
    this->CompileCode(subscr->left);
    this->CompileSlice(ast::CastNode<ast::Slice>(subscr->right));

    if (duplicate) {
        this->EmitOp2(OpCodes::DUP, 2);
        this->unit_->IncStack(2);
    }

    if (emit_subscr) {
        this->EmitOp(OpCodes::SUBSCR);
        this->unit_->DecStack();
    }
}

void Compiler::CompileSwitch(const ast::Switch *stmt, const std::string &name) {
    BasicBlock *tests = this->unit_->bb.current;
    BasicBlock *bodies = this->unit_->BlockNew();

    auto block = this->unit_->LBlockBegin(name, false);

    BasicBlock *ltest = tests;
    BasicBlock *lbody = bodies;
    BasicBlock *defcase = nullptr;

    bool as_if = stmt->test == nullptr;
    unsigned short idx = 0;

    if (!as_if)
        this->CompileCode(stmt->test);

    for (auto &swcase : stmt->cases) {
        auto cas = ast::CastNode<ast::Case>(swcase);

        if (!cas->tests.empty()) {
            bool compound = false;
            for (auto &test : cas->tests) {
                this->unit_->bb.current = ltest; // switch to test thread

                if (!as_if && (compound || idx > 0))
                    this->unit_->IncStack();

                this->CompileCode(test);

                if (!as_if) {
                    this->EmitOp(OpCodes::TEST);
                    this->unit_->DecStack();
                }

                this->CompileJump(OpCodes::JT, lbody);
                this->unit_->DecStack();

                ltest->flow.next = this->unit_->BlockNew();
                ltest = ltest->flow.next;
                compound = true;
            }
        } else {
            // Default case
            defcase = this->unit_->BlockNew();
            this->unit_->bb.current = defcase;
            if (!as_if)
                this->EmitOp(OpCodes::POP);
            this->CompileJump(OpCodes::JMP, lbody);
        }

        // Compile Body
        this->unit_->bb.current = lbody;

        this->unit_->symt.EnterSub();
        this->CompileCode(cas->body);
        this->unit_->symt.ExitSub();

        lbody = this->unit_->bb.current; // Update body after CompileCode, body may be changed (Eg: if, test, ...)

        if (++idx < stmt->cases.size()) {
            // Fix jmp / fallthrough
            if (ast::CastNode<ast::Block>(cas->body)->stmts.back()->type != ast::NodeType::FALLTHROUGH)
                this->CompileJump(OpCodes::JMP, block->end);

            lbody->flow.next = this->unit_->BlockNew();
            lbody = lbody->flow.next;
        }
    }

    // Fix jump for default case
    if (defcase != nullptr) {
        ltest->flow.next = defcase;
        defcase->flow.next = this->unit_->BlockNew();
        ltest = defcase->flow.next;
    } else {
        this->unit_->bb.current = ltest;
        this->CompileJump(OpCodes::JMP, block->end);
    }

    // TEST -> BODIES -> rest of program
    ltest->flow.next = bodies;
    this->unit_->bb.current = lbody;

    this->unit_->LBlockEnd();
}

void Compiler::CompileTest(const ast::Binary *test) {
    BasicBlock *end = this->unit_->BlockNew();

    while (true) {
        this->CompileCode(test->left);

        if (test->kind == scanner::TokenType::AND)
            this->CompileJump(OpCodes::JFOP, end);
        else if (test->kind == scanner::TokenType::OR)
            this->CompileJump(OpCodes::JTOP, end);
        else
            assert(false);

        this->unit_->BlockAsNextNew();

        if (test->right->type != ast::NodeType::TEST)
            break;

        test = ast::CastNode<ast::Binary>(test->right);
    }

    this->CompileCode(test->right);
    this->unit_->BlockAsNext(end);
}

void Compiler::CompileUpdate(const ast::Update *update) {
    unsigned int id;

#define EMIT_OP(kind, prefix)                                                           \
if (prefix)                                                                             \
    this->EmitOp(kind == scanner::TokenType::PLUS_PLUS ? OpCodes::INC : OpCodes::DEC);  \
this->EmitOp2(OpCodes::DUP, 1);                                                         \
this->unit_->IncStack();                                                                \
if (!prefix)                                                                            \
    this->EmitOp(kind == scanner::TokenType::PLUS_PLUS ? OpCodes::INC : OpCodes::DEC)

    switch (update->expr->type) {
        case ast::NodeType::IDENTIFIER: {
            auto ident = ast::CastNode<ast::Identifier>(update->expr);

            this->VariableLoad(ident->value);
            EMIT_OP(update->kind, update->prefix);
            this->VariableStore(ident->value);
            break;
        }
        case ast::NodeType::SUBSCRIPT: {
            auto subs = ast::CastNode<ast::Binary>(update->expr);

            this->CompileSubscr(subs, true, true);
            EMIT_OP(update->kind, update->prefix);
            this->EmitOp2(OpCodes::PB_HEAD, 3);
            this->EmitOp2(OpCodes::PB_HEAD, 3);
            this->EmitOp(OpCodes::STSUBSCR);
            this->unit_->DecStack(3);
            break;
        }
        case ast::NodeType::MEMBER: {
            auto member = ast::CastNode<ast::Member>(update->expr);

            id = this->CompileMember(member, true, true);
            EMIT_OP(update->kind, update->prefix);
            this->EmitOp2(OpCodes::PB_HEAD, 2);
            this->EmitOp2(OpCodes::PB_HEAD, 2);
            this->EmitOp4(OpCodes::STATTR, id);
            this->unit_->DecStack(2);
            break;
        }
        case ast::NodeType::SCOPE:
            id = this->CompileScope(ast::CastNode<ast::Scope>(update->expr), true, true);

            EMIT_OP(update->kind, update->prefix);
            this->EmitOp2(OpCodes::PB_HEAD, 2);
            this->EmitOp2(OpCodes::PB_HEAD, 2);
            this->EmitOp4(OpCodes::STSCOPE, id);
            this->unit_->DecStack(2);
            break;
        default:
            assert(false);
    }
#undef EMIT_OP
}

void Compiler::EmitOp(OpCodes code) {
    this->unit_->bb.current->AddInstr((Instr8) code);
}

void Compiler::EmitOp2(OpCodes code, unsigned char arg) {
    this->unit_->bb.current->AddInstr((Instr16) ((unsigned short) (arg << (unsigned char) 8) | (Instr8) code));
}

void Compiler::EmitOp4(OpCodes code, unsigned int arg) {
    assert(arg < 0x00FFFFFF); // TODO: too many argument!
    this->unit_->bb.current->AddInstr((Instr32) (arg << (unsigned char) 8) | (Instr8) code);
}

void Compiler::EmitOp4Flags(OpCodes code, unsigned char flags, unsigned short arg) {
    Instr32 istr = ((unsigned int) (flags << (unsigned char) 24)) |
                   ((unsigned short) (arg << (unsigned char) 8)) |
                   (Instr8) code;
    this->unit_->bb.current->AddInstr(istr);
}

void Compiler::EnterContext(const std::string &name, TUScope scope) {
    auto tu = AllocObject<TranslationUnit>(name, scope);
    tu->prev = this->unit_;
    this->unit_ = tu;

    // Create a new first BasicBlock
    this->unit_->BlockAsNextNew();
}

void Compiler::ExitContext() {
    auto tmp = this->unit_;
    this->unit_ = tmp->prev;
    FreeObject(tmp);
}

void Compiler::VariableLoad(const std::string &name) {
    // N.B. Unknown variable, by default does not raise an error,
    // but tries to load it from the global namespace.
    Symbol *sym;

    this->VariableLookupCreate(name, &sym);

    this->unit_->IncStack();

    if (this->unit_->scope == TUScope::FUNCTION || sym->nested > 0) {
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

void Compiler::VariableNew(const std::string &name, bool emit, unsigned char flags) {
    SymbolType type = SymbolType::VARIABLE;
    List *dest = this->unit_->names;
    ArObject *arname;
    Symbol *sym;
    bool inserted;

    if (((PropertyType) flags & PropertyType::CONST) == PropertyType::CONST)
        type = SymbolType::CONSTANT;

    sym = this->unit_->symt.Insert(name, type, &inserted);

    if (this->unit_->scope != TUScope::FUNCTION && sym->nested == 0) {
        if (emit) {
            this->EmitOp4Flags(OpCodes::NGV, flags, !inserted ? sym->id : dest->len);
            this->unit_->DecStack();
        }
        if (!inserted)
            return;
    } else {
        dest = this->unit_->locals;
        if (emit) {
            this->EmitOp2(OpCodes::STLC, dest->len);
            this->unit_->DecStack();
        }
    }

    if (!inserted)
        arname = ListGetItem(!sym->free ? this->unit_->names : this->unit_->enclosed, sym->id);
    else {
        // Convert string name to ArObject
        if ((arname = StringNew(name)) == nullptr)
            throw std::bad_alloc();
    }

    sym->id = dest->len;
    bool ok = ListAppend(dest, arname);
    Release(arname);
    if (!ok)
        throw std::bad_alloc();
}

void Compiler::VariableStore(const std::string &name) {
    Symbol *sym;

    if (this->VariableLookupCreate(name, &sym))
        sym->type = SymbolType::VARIABLE;

    this->unit_->DecStack();

    if (sym->declared && (this->unit_->scope == TUScope::FUNCTION || sym->nested > 0)) {
        this->EmitOp2(OpCodes::STLC, sym->id);
        return;
    } else if (sym->free) {
        this->EmitOp2(OpCodes::STENC, sym->id);
        return;
    }

    this->EmitOp4(OpCodes::STGBL, sym->id);
}

Compiler::Compiler() : Compiler(nullptr) {}

Compiler::Compiler(argon::object::Map *statics_globals) {
    if ((this->statics_globals_ = statics_globals) == nullptr) {
        if ((this->statics_globals_ = MapNew()) == nullptr)
            throw std::bad_alloc();
    }

    this->unit_ = nullptr;
}

Compiler::~Compiler() {
    while (this->unit_ != nullptr)
        this->ExitContext();
    Release(this->statics_globals_);
}

argon::object::Code *Compiler::Compile(std::istream *source) {
    Parser parser(source);

    auto program = parser.Parse();

    // Let's start creating a new context
    this->EnterContext(program->filename, TUScope::MODULE);

    // Cycle through program statements and call main compilation function
    for (auto &stmt:program->body)
        this->CompileCode(stmt);

    return this->Assemble();
}
