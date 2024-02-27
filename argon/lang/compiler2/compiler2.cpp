// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <string>

#include <argon/vm/datatype/nil.h>

#include <argon/lang/compiler2/compiler2.h>

using namespace argon::vm::datatype;
using namespace argon::lang::compiler2;
using namespace argon::lang::parser2;

#define CHECK_AST_NODE(expected, chk)                                                       \
do {                                                                                        \
    if(!AR_TYPEOF(chk, expected))                                                           \
        throw CompilerException(kCompilerErrors[0], (expected)->name, AR_TYPE_NAME(chk));   \
} while(0)

String *Compiler::MkImportName(const node::Unary *literal) {
    auto *mod_name = (String *) literal->value;

    const unsigned char *end = ARGON_RAW_STRING(mod_name) + ARGON_RAW_STRING_LENGTH(mod_name);
    unsigned int idx = 0;

    while (idx < ARGON_RAW_STRING_LENGTH(mod_name) && std::isalnum(*((end - idx) - 1)))
        idx++;

    if (!std::isalpha(*(end - idx))) {
        // TODO CompilerError[0]
        ErrorFormat("CompilerError", kCompilerErrors[8], ARGON_RAW_STRING(mod_name));
        throw DatatypeException();
    }

    auto *ret = StringNew((const char *) end - idx, idx);
    if (ret == nullptr)
        throw DatatypeException();

    return ret;
}

SymbolT *Compiler::IdentifierLookupOrCreate(String *id, argon::lang::compiler2::SymbolType type) {
    auto *dst = this->unit_->names;

    auto *sym = this->unit_->symt->SymbolLookup(id);
    if (sym == nullptr) {
        if ((sym = this->unit_->symt->SymbolInsert(id, type)) == nullptr)
            throw DatatypeException();

        if (this->unit_->IsFreeVar(id)) {
            dst = this->unit_->enclosed;
            sym->free = true;
        }

        sym->id = (short) dst->length;

        if (!ListAppend(dst, (ArObject *) id)) {
            Release(sym);

            throw DatatypeException();
        }
    }

    return sym;
}

void Compiler::Compile(const node::Node *node) {
    switch (node->node_type) {
        case node::NodeType::ASSERTION:
            this->CompileAssertion((const node::Binary *) node);
            break;
        case node::NodeType::ASSIGNMENT:
            this->CompileAssignment((const node::Assignment *) node);
            break;
        case node::NodeType::BLOCK:
            this->CompileBlock(node, true);
            break;
        case node::NodeType::CALL:
            this->CompileCall((const node::Call *) node);
            break;
        case node::NodeType::EXPRESSION:
            this->Expression((const node::Node *) ((const node::Unary *) node)->value);
            this->unit_->EmitPOP();
            break;
        case node::NodeType::FOR:
            this->CompileFor((const node::Loop *) node);
            break;
        case node::NodeType::FOREACH:
            this->CompileForEach((const node::Loop *) node);
            break;
        case node::NodeType::FUNCTION:
            this->CompileFunction((const node::Function *) node);
            break;
        case node::NodeType::JUMP:
            this->CompileJump((const node::Unary *) node);
            break;
        case node::NodeType::IF:
            this->CompileIF((const node::Branch *) node);
            break;
        case node::NodeType::IMPORT:
            this->CompileImport((const node::Import *) node);
            break;
        case node::NodeType::LABEL: {
            auto *label = (const node::Binary *) node;

            CHECK_AST_NODE(node::type_ast_binary_, label);

            this->unit_->JBPush((String *) label->left, JBlockType::LABEL);

            this->Compile((const node::Node *) label->right);

            this->unit_->JBPop();

            break;
        }
        case node::NodeType::LOOP:
            this->CompileLoop((const node::Loop *) node);
            break;
        case node::NodeType::PANIC:
            CHECK_AST_NODE(node::type_ast_unary_, node);

            this->Expression((const node::Node *) ((const node::Unary *) node)->value);

            this->unit_->Emit(vm::OpCode::PANIC, &node->loc);

            break;
        case node::NodeType::RETURN: {
            auto *ret = (const node::Unary *) node;

            CHECK_AST_NODE(node::type_ast_unary_, ret);

            if (ret->value != nullptr)
                this->Expression((const node::Node *) ret->value);
            else
                this->LoadStaticNil(&ret->loc, true);

            this->unit_->Emit(vm::OpCode::RET, &ret->loc);

            break;
        }
        case node::NodeType::STRUCT:
        case node::NodeType::TRAIT:
            assert(false);
        case node::NodeType::SWITCH:
            assert(false);
        case node::NodeType::SYNC_BLOCK:
            this->CompileSyncBlock((const node::Binary *) node);
            break;
        case node::NodeType::VARDECL:
            this->CompileVarDecl((const node::Assignment *) node);
            break;
        case node::NodeType::YIELD:
            CHECK_AST_NODE(node::type_ast_unary_, node);

            if (this->unit_->symt->type != SymbolType::FUNC
                && this->unit_->symt->type != SymbolType::GENERATOR)
                throw CompilerException(kStandardError[5]);

            this->unit_->symt->type = SymbolType::GENERATOR;

            this->Expression((const node::Node *) ((const node::Unary *) node)->value);

            this->unit_->Emit(vm::OpCode::YLD, &node->loc);

            break;
        default:
            throw CompilerException(kCompilerErrors[1], (int) node->node_type, __FUNCTION__);
    }
}

void Compiler::CompileAssertion(const node::Binary *binary) {
    ArObject *tmp = nullptr;
    BasicBlock *end;

    CHECK_AST_NODE(node::type_ast_assertion_, binary);

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        this->Expression((const node::Node *) binary->left);

        this->unit_->Emit(vm::OpCode::JT, 0, end, &binary->loc);

        this->unit_->BlockNew();

        // Assertion failed
        this->LoadStatic((ArObject *) type_error_, &binary->loc, true, true);

        this->LoadStaticAtom(kAssertionError[0], &binary->loc, true);

        if (binary->right == nullptr) {
            tmp = (ArObject *) StringIntern("");

            this->LoadStatic(tmp, &binary->loc, true, true);

            Release(tmp);
        } else
            this->Expression((const node::Node *) binary->right);

        this->unit_->Emit(vm::OpCode::CALL, (unsigned char) vm::OpCodeCallMode::FASTCALL, 2, &binary->loc);

        this->unit_->Emit(vm::OpCode::PANIC, &binary->loc);
    } catch (...) {
        Release(tmp);
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileAssignment(const node::Assignment *assignment) {
    CHECK_AST_NODE(node::type_ast_assignment_, assignment);

    if (assignment->token_type != scanner::TokenType::EQUAL) {
        this->CompileAugAssignment(assignment);
        return;
    }

    this->CompileStore((const node::Node *) assignment->name, (const node::Node *) assignment->value);
}

void Compiler::CompileAugAssignment(const node::Assignment *assignment) {
    vm::OpCode opcode;

    // Select opcode
    switch (assignment->token_type) {
        case scanner::TokenType::ASSIGN_ADD:
            opcode = vm::OpCode::IPADD;
            break;
        case scanner::TokenType::ASSIGN_SUB:
            opcode = vm::OpCode::IPSUB;
            break;
        default:
            throw CompilerException(kCompilerErrors[6]);
    }

#define COMPILE_OP()                                            \
    this->Expression((const node::Node*) assignment->value);    \
    this->unit_->Emit(opcode, &assignment->loc)

    auto *left = (const node::Node *) assignment->name;
    switch (left->node_type) {
        case node::NodeType::IDENTIFIER:
            this->LoadIdentifier((const node::Unary *) left);

            COMPILE_OP();

            this->StoreVariable((String *) ((const node::Unary *) left)->value, &assignment->loc);
            break;
        case node::NodeType::INDEX:
        case node::NodeType::SLICE:
            this->CompileSubscr((const node::Subscript *) left, true, true);

            COMPILE_OP();

            this->unit_->Emit(vm::OpCode::STSUBSCR, &assignment->loc);
            break;
        case node::NodeType::SELECTOR: {
            auto idx = this->CompileSelector((const node::Binary *) left, true, true);

            COMPILE_OP();

            if (left->token_type == scanner::TokenType::SCOPE) {
                this->unit_->Emit(vm::OpCode::STSCOPE, idx, nullptr, &assignment->loc);
                break;
            }

            this->unit_->Emit(vm::OpCode::STATTR, idx, nullptr, &assignment->loc);
            break;
        }
        default:
            throw CompilerException(kCompilerErrors[1], (int) left->node_type, __FUNCTION__);
    }

#undef COMPILE_OP
}

void Compiler::CompileFor(const node::Loop *loop) {
    BasicBlock *begin;
    BasicBlock *end;

    CHECK_AST_NODE(node::type_ast_loop_, loop);

    if (!this->unit_->symt->NewNestedTable())
        throw DatatypeException();

    if (loop->init != nullptr)
        this->Compile(loop->init);

    this->unit_->BlockNew();

    begin = this->unit_->bbb.current;

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        this->unit_->JBPush(begin, end);

        this->Expression(loop->test);

        this->unit_->Emit(vm::OpCode::JF, 0, end, &loop->test->loc);

        this->unit_->BlockNew();

        this->CompileBlock(loop->body, false);

        if (loop->inc != nullptr) {
            auto *inc = loop->inc;
            switch (inc->node_type) {
                case node::NodeType::ASSIGNMENT:
                    this->CompileAssignment((const node::Assignment *) inc);
                    break;
                case node::NodeType::CALL:
                    this->CompileCall((const node::Call *) inc);
                    break;
                case node::NodeType::UPDATE:
                    this->CompileUpdate((const node::Unary *) inc);
                    this->unit_->Emit(vm::OpCode::POP, nullptr);
                    break;
                default:
                    throw CompilerException(kCompilerErrors[1], (int) inc->node_type, __FUNCTION__);
            }
        }

        this->unit_->Emit(vm::OpCode::JMP, 0, begin, nullptr);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->JBPop();

    SymbolExitNested(this->unit_->symt);

    this->unit_->BlockAppend(end);
}

void Compiler::CompileForEach(const node::Loop *loop) {
    BasicBlock *begin;
    BasicBlock *end;

    CHECK_AST_NODE(node::type_ast_loop_, loop);

    if (!this->unit_->symt->NewNestedTable())
        throw DatatypeException();

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        if (loop->init != nullptr)
            this->Compile(loop->init);

        this->Expression(loop->test);

        this->unit_->Emit(vm::OpCode::LDITER, &loop->loc);

        this->unit_->BlockNew();

        begin = this->unit_->bbb.current;

        this->unit_->JBPush(begin, end, 1);

        this->unit_->Emit(vm::OpCode::NXT, nullptr);
        this->unit_->Emit(vm::OpCode::JEX, 0, end, nullptr);

        this->unit_->BlockNew();

        if (loop->inc->node_type == node::NodeType::IDENTIFIER)
            this->StoreVariable((const node::Unary *) loop->inc);
        else if (loop->inc->node_type == node::NodeType::TUPLE)
            this->CompileUnpack((List *) ((const node::Unary *) loop->inc)->value, &loop->inc->loc);

        this->CompileBlock(loop->body, false);

        this->unit_->Emit(vm::OpCode::JMP, 0, begin, nullptr);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->JBPop();

    SymbolExitNested(this->unit_->symt);

    this->unit_->BlockAppend(end);

    this->unit_->DecrementStack(1); // JEX remove iterator from eval stack
}

void Compiler::CompileJump(const node::Unary *jump) {
    auto *jb = this->unit_->jblock;
    unsigned short pops = 0;

    CHECK_AST_NODE(node::type_ast_jump_, jump);

    if (jump->token_type != scanner::TokenType::KW_BREAK && jump->token_type != scanner::TokenType::KW_CONTINUE)
        throw CompilerException(kCompilerErrors[2], (int) jump->token_type, __FUNCTION__);

    if (jump->value != nullptr) {
        jb = this->unit_->JBFindLabel((const String *) jump->value, pops);
        if (jb == nullptr)
            throw CompilerException(kCompilerErrors[7],
                                    ARGON_RAW_STRING((String *) jump->value)),
                    (jump->token_type == scanner::TokenType::KW_BREAK ? "breaked" : "continued");
    }

    auto *dst = jb->end;

#define POPN(n)                 \
    do{for(auto i=0;i<(n);i++)this->unit_->EmitPOP();}while(0)

    if (jump->token_type == scanner::TokenType::KW_BREAK) {
        POPN(pops);

        // Don't decrease the stack size
        this->unit_->IncrementStack(pops);
    } else if (jump->token_type == scanner::TokenType::KW_CONTINUE) {
        pops -= jb->pops;

        POPN(pops);

        // Don't decrease the stack size
        this->unit_->IncrementStack(pops);

        dst = jb->begin;
    } else
        throw CompilerException(kCompilerErrors[2], (int) jump->token_type, __FUNCTION__);

#undef POPN

    // If continue/break is inside a sync block, release the resource before executing continue/break
    for (auto *cursor = this->unit_->jblock; cursor != jb; cursor = cursor->prev) {
        if (cursor->type != JBlockType::SYNC)
            continue;

        this->unit_->ExitSync();

        // Don't decrease sync_stack size
        this->unit_->sync_stack.current++;
    }

    this->unit_->Emit(vm::OpCode::JMP, 0, dst, nullptr);
}

void Compiler::CompileImport(const node::Import *imp) {
    ARC iter;
    ARC tmp;

    CHECK_AST_NODE(node::type_ast_import_, imp);

    if (imp->mod != nullptr) {
        auto idx = this->LoadStatic((const node::Unary *) imp->mod, true, false);
        this->unit_->Emit(vm::OpCode::IMPMOD, idx, nullptr, &imp->loc);
    }

    if (imp->names == nullptr) {
        this->unit_->Emit(vm::OpCode::IMPALL, &imp->loc);
        return;
    }

    iter = IteratorGet(imp->names, false);
    if (!iter)
        throw DatatypeException();

    while ((tmp = IteratorNext(iter.Get())))
        this->CompileImportAlias((const node::Binary *) tmp.Get(), imp->mod != nullptr);

    if (imp->mod != nullptr)
        this->unit_->EmitPOP();
}

void Compiler::CompileImportAlias(const node::Binary *binary, bool impfrm) {
    ARC name;
    int idx = 0;

    CHECK_AST_NODE(node::type_ast_import_name_, binary);

    if (impfrm)
        idx = this->LoadStatic(binary->left, &((const node::Unary *) binary->left)->loc, true, false);
    else
        idx = this->LoadStatic((const node::Unary *) binary->left, true, false);

    this->unit_->Emit(impfrm ? vm::OpCode::IMPFRM : vm::OpCode::IMPMOD, idx, nullptr, &binary->loc);

    if (binary->right != nullptr)
        name = binary->right;

    if (!name) {
        if (impfrm)
            name = ((const node::Unary *) binary->left)->value;
        else
            name = Compiler::MkImportName((const node::Unary *) binary->left);
    }

    this->IdentifierNew((String *) name.Get(), &binary->loc, SymbolType::CONSTANT, AttributeFlag::CONST, true);
}

void Compiler::CompileIF(const node::Branch *branch) {
    BasicBlock *orelse;
    BasicBlock *end;

    CHECK_AST_NODE(node::type_ast_branch_, branch);

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    orelse = end;

    try {
        this->Expression(branch->test);

        this->unit_->Emit(vm::OpCode::JF, 0, orelse, &branch->loc);

        this->unit_->BlockNew();

        this->CompileBlock(branch->body, this->unit_->symt->type != SymbolType::MODULE);

        if (branch->orelse != nullptr) {
            if ((end = BasicBlockNew()) == nullptr)
                throw DatatypeException();

            this->unit_->Emit(vm::OpCode::JMP, 0, end, nullptr);

            this->unit_->BlockAppend(orelse);
            orelse = nullptr; // Avoid releasing it in case of an exception.

            this->Compile(branch->orelse);
        }
    } catch (...) {
        if (orelse != end) {
            BasicBlockDel(orelse);
            BasicBlockDel(end);
        } else
            BasicBlockDel(end);

        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileLoop(const node::Loop *loop) {
    BasicBlock *begin;
    BasicBlock *end;

    CHECK_AST_NODE(node::type_ast_loop_, loop);

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    if (!this->unit_->BlockNew())
        throw DatatypeException();

    begin = this->unit_->bbb.current;

    try {
        this->unit_->JBPush(begin, end);

        if (loop->test != nullptr) {
            this->Expression(loop->test);

            this->unit_->Emit(vm::OpCode::JF, 0, end, &loop->test->loc);
        }

        this->CompileBlock(loop->body, true);

        this->unit_->Emit(vm::OpCode::JMP, 0, begin, nullptr);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->JBPop();

    this->unit_->BlockAppend(end);
}

void Compiler::CompileStore(const node::Node *node, const node::Node *value) {
    switch (node->node_type) {
        case node::NodeType::IDENTIFIER:
            if (value != nullptr)
                this->Expression(value);

            this->StoreVariable(((const node::Unary *) node));
            break;
        case node::NodeType::INDEX:
        case node::NodeType::SLICE:
            this->CompileSubscr((const node::Subscript *) node, false, false);

            if (value != nullptr)
                this->Expression(value);
            else
                this->unit_->Emit(vm::OpCode::MTH, 2, nullptr, &value->loc);

            this->unit_->Emit(vm::OpCode::STSUBSCR, &node->loc);
            break;
        case node::NodeType::SELECTOR: {
            auto idx = this->CompileSelector((const node::Binary *) node, false, false);

            if (value != nullptr)
                this->Expression(value);
            else
                this->unit_->Emit(vm::OpCode::MTH, 1, nullptr, &value->loc);

            if (node->token_type == scanner::TokenType::SCOPE) {
                this->unit_->Emit(vm::OpCode::STSCOPE, idx, nullptr, &node->loc);
                return;
            }

            this->unit_->Emit(vm::OpCode::STATTR, idx, nullptr, &node->loc);
            break;
        }
        case node::NodeType::TUPLE:
            if (value != nullptr)
                this->Expression(value);

            this->CompileUnpack((List *) ((const node::Unary *) node)->value, &node->loc);
            break;
        default:
            throw CompilerException(kCompilerErrors[1], (int) node->node_type, __FUNCTION__);
    }
}

void Compiler::CompileSyncBlock(const node::Binary *binary) {
    CHECK_AST_NODE(node::type_ast_sync_, binary);

    this->Expression((const node::Node *) binary->left);

    this->unit_->EnterSync(&((const node::Binary *) binary->left)->loc);

    this->unit_->JBPush(nullptr, JBlockType::SYNC);

    this->CompileBlock((const node::Node *) binary->right, true);

    this->unit_->ExitSync();

    this->unit_->JBPop();
}

void Compiler::CompileUnpack(List *list, const scanner::Loc *loc) {
    ARC iter;
    ARC tmp;

    iter = IteratorGet((ArObject *) list, false);
    if (!iter)
        throw DatatypeException();

    this->unit_->Emit(vm::OpCode::UNPACK, loc);

    auto *instr = this->unit_->bbb.current->instr.tail;

    auto items = 0;

    while ((tmp = IteratorNext(iter.Get()))) {
        auto *id = (const node::Node *) tmp.Get();

        this->unit_->IncrementStack(1);

        this->CompileStore(id, nullptr);

        items++;
    }

    instr->oparg = (unsigned int) items;
}

void Compiler::CompileVarDecl(const node::Assignment *assignment) {
    SymbolType s_type = SymbolType::VARIABLE;
    AttributeFlag a_flags{};

    CHECK_AST_NODE(node::type_ast_vardecl_, assignment);

    if (assignment->constant) {
        s_type = SymbolType::CONSTANT;
        a_flags = AttributeFlag::CONST;
    }

    if (assignment->pub)
        a_flags |= AttributeFlag::PUBLIC;

    if (assignment->weak) {
        if (assignment->constant)
            throw CompilerException(kCompilerErrors[9]);

        a_flags |= AttributeFlag::WEAK;
    }

    if (!assignment->multi) {
        if (assignment->value == nullptr) {
            if (assignment->constant)
                throw CompilerException(kCompilerErrors[10]);

            this->LoadStaticNil(&assignment->loc, true);
        } else
            this->Expression((const node::Node *) assignment->value);

        this->IdentifierNew((String *) assignment->name, &assignment->loc, s_type, a_flags, true);

        return;
    }

    ARC iter;
    ARC tmp;

    Instr *unpack = nullptr;
    unsigned short v_count = 0;

    if (assignment->value != nullptr) {
        this->Expression((const node::Node *) assignment->value);

        this->unit_->Emit(vm::OpCode::UNPACK, 0, nullptr, &assignment->loc);

        unpack = this->unit_->bbb.current->instr.tail;
    }

    iter = IteratorGet(assignment->name, false);
    if (!iter)
        throw DatatypeException();

    while ((tmp = IteratorNext(iter.Get()))) {
        if (assignment->value == nullptr)
            this->LoadStaticNil(&assignment->loc, true);
        else
            this->unit_->IncrementStack(1);

        this->IdentifierNew((String *) tmp.Get(), &assignment->loc, s_type, a_flags, true);

        v_count++;
    }

    if (assignment->value != nullptr) {
        this->unit_->IncrementRequiredStack(v_count);
        unpack->oparg = v_count;
    }
}

// *********************************************************************************************************************
// EXPRESSION-ZONE
// *********************************************************************************************************************

int Compiler::CompileSelector(const node::Binary *binary, bool dup, bool emit) {
    const auto *cursor = binary;
    int idx;

    CHECK_AST_NODE(node::type_ast_selector_, binary);

    auto deep = 0;
    while (((const node::Node *) cursor->left)->node_type == node::NodeType::SELECTOR) {
        cursor = (const node::Binary *) cursor->left;
        deep++;
    }

    this->Expression((const node::Node *) cursor->left);

    do {
        vm::OpCode code;

        switch (cursor->token_type) {
            case scanner::TokenType::SCOPE:
                code = vm::OpCode::LDSCOPE;
                break;
            case scanner::TokenType::DOT:
                code = vm::OpCode::LDATTR;
                break;
            case scanner::TokenType::QUESTION_DOT:
                code = vm::OpCode::LDATTR;
                this->unit_->Emit(vm::OpCode::JNIL, 0, this->unit_->jblock->end, &cursor->loc);
                break;
            default:
                throw CompilerException(kCompilerErrors[2], cursor->token_type, __FUNCTION__);
        }

        idx = this->LoadStatic((node::ArObject *) cursor->right, &cursor->loc, true, false);

        if (dup && deep == 0)
            this->unit_->Emit(vm::OpCode::DUP, 1, nullptr, nullptr);

        if (deep > 0 || emit)
            this->unit_->Emit(code, idx, nullptr, &cursor->loc);

        deep--;
        cursor = binary;
        for (int i = 0; i < deep; i++)
            cursor = (const node::Binary *) cursor->left;
    } while (deep >= 0);

    return idx;
}

int Compiler::LoadStatic(ArObject *object, const scanner::Loc *loc, bool store, bool emit) {
    auto *value = IncRef(object);
    ArObject *tmp;

    int idx = -1;

    if (store) {
        // Check if value are already present in TranslationUnit
        if ((tmp = DictLookup(this->unit_->statics_map, value)) == nullptr) {
            // Value not found in the current TranslationUnit, trying in global_statics
            if ((tmp = DictLookup(this->static_globals_, value)) != nullptr) {
                // Recover already existing object and discard the actual one
                Release(value);
                value = tmp;
            } else if (!DictInsert(this->static_globals_, value, value)) {
                Release(value);

                throw DatatypeException();
            }

            auto *index = UIntNew(this->unit_->statics->length);
            if (index == nullptr) {
                Release(value);

                throw DatatypeException();
            }

            if (!DictInsert(this->unit_->statics_map, value, (ArObject *) index)) {
                Release(value);
                Release(index);

                throw DatatypeException();
            }

            Release(index);
        } else {
            idx = (int) ((Integer *) tmp)->sint;
            Release(tmp);
        }
    }

    if (!store || idx == -1) {
        idx = (int) this->unit_->statics->length;
        if (!ListAppend(this->unit_->statics, value)) {
            Release(value);

            throw DatatypeException();
        }
    }

    Release(value);

    if (emit)
        this->unit_->Emit(vm::OpCode::LSTATIC, idx, nullptr, loc);

    return idx;
}

int Compiler::LoadStatic(const node::Unary *literal, bool store, bool emit) {
    CHECK_AST_NODE(node::type_ast_literal_, literal);

    return this->LoadStatic(literal->value, &literal->loc, store, emit);
}

int Compiler::LoadStaticAtom(const char *key, const scanner::Loc *loc, bool emit) {
    ARC atom;

    atom = AtomNew(key);
    if (!atom)
        throw DatatypeException();

    return this->LoadStatic(atom.Get(), loc, false, emit);
}

int Compiler::LoadStaticNil(const scanner::Loc *loc, bool emit) {
    return this->LoadStatic((ArObject *) Nil, loc, true, emit);
}

String *Compiler::MakeFnName() {
    String *name;

    if (this->unit_->name != nullptr)
        name = StringFormat("%s$%d", ARGON_RAW_STRING(this->unit_->name), this->unit_->anon_count++);
    else
        name = StringFormat("$%d", this->unit_->anon_count++);

    if (name == nullptr)
        throw DatatypeException();

    return name;
}

void Compiler::IdentifierNew(String *name, const scanner::Loc *loc, SymbolType type, AttributeFlag aflags, bool emit) {
    ARC sym;

    if (StringEqual(name, "_"))
        throw CompilerException(kCompilerErrors[3], "_");

    sym = (ArObject *) this->unit_->symt->SymbolInsert(name, type);
    if (!sym)
        throw DatatypeException();

    auto *dest = this->unit_->names;
    auto *p_sym = (SymbolT *) sym.Get();

    p_sym->declared = true;

    if (this->unit_->symt->type == SymbolType::STRUCT || this->unit_->symt->type == SymbolType::TRAIT) {
        this->LoadStatic((ArObject *) name, loc, true, true);

        this->unit_->Emit(vm::OpCode::TSTORE, (unsigned char) aflags, nullptr, loc);
        return;
    }

    if (p_sym->nested == 0) {
        auto id = (unsigned short) (p_sym->id >= 0 ? p_sym->id : dest->length);

        if (emit)
            this->unit_->Emit(vm::OpCode::NGV, (unsigned char) aflags, id, loc);

        if (p_sym->id >= 0)
            return;
    } else {
        dest = this->unit_->locals;

        if (emit)
            this->unit_->Emit(vm::OpCode::STLC, (int) dest->length, nullptr, loc);
    }

    ArObject *arname;

    if (p_sym->id >= 0)
        arname = ListGet(!p_sym->free ? this->unit_->names : this->unit_->enclosed, p_sym->id);
    else
        arname = (ArObject *) IncRef(name);

    p_sym->id = (short) dest->length;

    if (!ListAppend(dest, arname)) {
        Release(arname);

        throw DatatypeException();
    }

    Release(arname);
}

void Compiler::IdentifierNew(const node::Unary *id, SymbolType type, AttributeFlag aflags, bool emit) {
    CHECK_AST_NODE(node::type_ast_identifier_, id);

    this->IdentifierNew((String *) id->value, &id->loc, type, aflags, emit);
}

void Compiler::LoadIdentifier(String *identifier, const scanner::Loc *loc) {
    // N.B. Unknown variable, by default does not raise an error,
    // but tries to load it from the global namespace.
    if (StringEqual(identifier, (const char *) "_"))
        throw CompilerException(kCompilerErrors[3], identifier);

    auto *sym = this->IdentifierLookupOrCreate(identifier, SymbolType::VARIABLE);

    auto sym_id = sym->id;
    auto nested = sym->nested;
    auto declared = sym->declared;
    auto free = sym->free;

    Release(sym);

    auto scope = this->unit_->symt->type;
    if (scope != SymbolType::STRUCT
        && scope != SymbolType::TRAIT
        && nested > 0) {
        if (declared) {
            this->unit_->Emit(vm::OpCode::LDLC, sym_id, nullptr, loc);
            return;
        } else if (free) {
            this->unit_->Emit(vm::OpCode::LDENC, sym_id, nullptr, loc);
            return;
        }
    }

    this->unit_->Emit(vm::OpCode::LDGBL, sym_id, nullptr, loc);
}

void Compiler::LoadIdentifier(const node::Unary *identifier) {
    CHECK_AST_NODE(node::type_ast_identifier_, identifier);

    this->LoadIdentifier((String *) identifier->value, &identifier->loc);
}

void Compiler::CompileBlock(const node::Node *node, bool sub) {
    ARC iter;
    ARC stmt;

    CHECK_AST_NODE(node::type_ast_unary_, node);

    iter = IteratorGet(((const node::Unary *) node)->value, false);
    if (!iter)
        throw DatatypeException();

    if (sub && !this->unit_->symt->NewNestedTable())
        throw DatatypeException();

    while ((stmt = IteratorNext(iter.Get())))
        this->Compile((const node::Node *) stmt.Get());

    if (sub)
        SymbolExitNested(this->unit_->symt);
}

void Compiler::CompileCall(const node::Call *call) {
    ARC iter;
    ARC param;

    unsigned short args = 0;

    CHECK_AST_NODE(node::type_ast_call_, call);

    if (call->left->node_type == node::NodeType::SELECTOR &&
        call->left->token_type != scanner::TokenType::SCOPE) {
        auto idx = this->CompileSelector((const node::Binary *) call->left, false, false);

        this->unit_->Emit(vm::OpCode::LDMETH, idx, nullptr, &call->left->loc);

        args = 1;
    } else
        this->Expression(call->left);

    auto op = vm::OpCode::CALL;
    auto mode = vm::OpCodeCallMode::FASTCALL;

    if (call->args != nullptr)
        this->CompileCallPositional(call->args, args, mode);

    if (call->kwargs != nullptr)
        this->CompileCallKWArgs(call->kwargs, args, mode);

    if (call->token_type == scanner::TokenType::KW_DEFER)
        op = vm::OpCode::DFR;
    else if (call->token_type == scanner::TokenType::KW_SPAWN)
        op = vm::OpCode::SPW;

    this->unit_->Emit(op, (unsigned char) mode, args, &call->loc);
}

void Compiler::CompileCallKWArgs(List *args, unsigned short &count, vm::OpCodeCallMode &mode) {
    ARC iter;
    ARC arg;

    iter = IteratorGet((ArObject *) args, false);
    if (!iter)
        throw DatatypeException();

    bool dict_expansion = false;
    int items = 0;

    // key = value
    while ((arg = IteratorNext(iter.Get()))) {
        const auto *tmp = (const node::Parameter *) arg.Get();

        if (tmp->node_type == node::NodeType::KWARG) {
            dict_expansion = true;
            continue;
        }

        this->LoadStatic((ArObject *) tmp->id, &tmp->loc, false, true);

        if (tmp->value != nullptr)
            this->Expression(tmp->value);
        else
            this->LoadStaticNil(&tmp->loc, true);

        items += 2;
    }

    if (items > 0)
        this->unit_->Emit(vm::OpCode::MKDT, items, nullptr, nullptr);

    if (dict_expansion) {
        iter = IteratorGet((ArObject *) args, false);
        if (!iter)
            throw DatatypeException();

        // &kwargs
        while ((arg = IteratorNext(iter.Get()))) {
            const auto *tmp = (const node::Parameter *) arg.Get();

            if (tmp->node_type != node::NodeType::KWARG)
                continue;

            this->Expression(tmp->value);

            if (items > 0)
                this->unit_->Emit(vm::OpCode::DTMERGE, nullptr);

            items++;
        }
    }

    mode |= vm::OpCodeCallMode::KW_PARAMS;

    if (ENUMBITMASK_ISTRUE(mode, vm::OpCodeCallMode::REST_PARAMS)) {
        this->unit_->Emit(vm::OpCode::PLT, nullptr);
        return;
    }

    count++;
}

void Compiler::CompileCallPositional(List *args, unsigned short &count, vm::OpCodeCallMode &mode) {
    ARC iter;
    ARC arg;

    iter = IteratorGet((ArObject *) args, false);
    if (!iter)
        throw DatatypeException();

    while ((arg = IteratorNext(iter.Get()))) {
        const auto *tmp = (const node::Unary *) arg.Get();

        if (tmp->node_type == node::NodeType::SPREAD) {
            if (ENUMBITMASK_ISFALSE(mode, vm::OpCodeCallMode::REST_PARAMS))
                this->unit_->Emit(vm::OpCode::MKLT, count, nullptr, &tmp->loc);

            this->Expression((const node::Node *) tmp->value);

            this->unit_->Emit(vm::OpCode::EXTD, nullptr);

            mode |= vm::OpCodeCallMode::REST_PARAMS;
        } else {
            this->Expression((const node::Node *) tmp);

            if (ENUMBITMASK_ISTRUE(mode, vm::OpCodeCallMode::REST_PARAMS))
                this->unit_->Emit(vm::OpCode::PLT, nullptr);
        }

        count++;
    }
}

void Compiler::CompileDLST(const node::Unary *unary) {
    ARC iter;
    ARC tmp;

    vm::OpCode code;
    int items = 0;

    CHECK_AST_NODE(node::type_ast_unary_, unary);

    iter = IteratorGet(unary->value, false);
    if (!iter)
        throw DatatypeException();

    while ((tmp = IteratorNext(iter.Get()))) {
        this->Expression((node::Node *) tmp.Get());
        items++;
    }

    switch (unary->node_type) {
        case node::NodeType::DICT:
            code = vm::OpCode::MKDT;
            break;
        case node::NodeType::LIST:
            code = vm::OpCode::MKLT;
            break;
        case node::NodeType::SET:
            code = vm::OpCode::MKST;
            break;
        case node::NodeType::TUPLE:
            code = vm::OpCode::MKTP;
            break;
        default:
            throw CompilerException(kCompilerErrors[2], (int) unary->token_type, __FUNCTION__);
    }

    this->unit_->Emit(code, items, nullptr, &unary->loc);
}

void Compiler::CompileElvis(const node::Binary *binary) {
    CHECK_AST_NODE(node::type_ast_binary_, binary);

    this->Expression((node::Node *) binary->left);

    auto *end = BasicBlockNew();
    if (end == nullptr)
        throw DatatypeException();

    try {
        this->unit_->Emit(vm::OpCode::JTOP, 0, end, &binary->loc);

        this->Expression((node::Node *) binary->right);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileFunction(const node::Function *func) {
    ARC code;
    ARC fname;

    FunctionFlags flags{};

    CHECK_AST_NODE(node::type_ast_function_, func);

    if (func->name != nullptr)
        fname = func->name;
    else
        fname = this->MakeFnName();

    if (this->unit_->symt->type == SymbolType::STRUCT
        || this->unit_->symt->type == SymbolType::TRAIT)
        flags = FunctionFlags::STATIC;

    this->EnterScope((String *) fname.Get(), SymbolType::FUNC);

    unsigned short p_count = 0;

    this->CompileFunctionParams(func->params, p_count, flags);

    if (func->body != nullptr)
        this->CompileBlock(func->body, false);
    else
        this->CompileFunctionDefBody(func, (String *) fname.Get());

    if (!this->unit_->bbb.CheckLastInstr(vm::OpCode::RET)) {
        // If the function is empty or the last statement is not return,
        // forcefully enter return as last statement

        if (this->unit_->symt->type == SymbolType::GENERATOR)
            this->LoadStaticAtom("stop", nullptr, true);
        else
            this->LoadStaticNil(nullptr, true);

        this->unit_->Emit(vm::OpCode::RET, nullptr);
    }

    // Update function flags if is a generator
    if (this->unit_->symt->type == SymbolType::GENERATOR)
        flags |= FunctionFlags::GENERATOR;

    // TODO assemble

    this->ExitScope();

    this->CompileFunctionClosure((const Code *) code.Get(), &func->loc, flags);

    this->CompileFunctionDefArgs(func->params, &func->loc, flags);

    this->LoadStatic(code.Get(), &func->loc, false, true);

    if (func->async)
        flags |= FunctionFlags::ASYNC;

    this->unit_->Emit(vm::OpCode::MKFN, (unsigned char) flags, p_count, &func->loc);

    if (func->name != nullptr) {
        auto aflags = AttributeFlag::CONST;

        if (func->pub)
            aflags |= AttributeFlag::PUBLIC;

        this->IdentifierNew(func->name, &func->loc, SymbolType::FUNC, aflags, true);
    }
}

void Compiler::CompileFunctionClosure(const Code *code, const scanner::Loc *loc, FunctionFlags &flags) {
    if (code->enclosed->length == 0) {
        this->unit_->Emit(vm::OpCode::PSHN, loc);
        return;
    }

    for (ArSize i = 0; i < code->enclosed->length; i++)
        this->LoadIdentifier((String *) code->enclosed->objects[i], loc);

    this->unit_->Emit(vm::OpCode::MKLT, (int) code->enclosed->length, nullptr, loc);
}

void Compiler::CompileFunctionDefArgs(List *params, const scanner::Loc *loc, FunctionFlags &flags) {
    ARC iter;
    ARC tmp;

    unsigned short def_count = 0;

    if (params == nullptr) {
        this->unit_->Emit(vm::OpCode::PSHN, loc);

        return;
    }

    iter = IteratorGet((ArObject *) params, false);
    if (!iter)
        throw DatatypeException();

    while ((tmp = IteratorNext(iter.Get()))) {
        const auto *param = (node::Parameter *) tmp.Get();

        CHECK_AST_NODE(node::type_ast_parameter_, param);

        if (param->value != nullptr) {
            this->Expression(param->value);
            def_count++;
        }

        // Sanity check
        if (def_count > 0 && param->node_type == node::NodeType::ARGUMENT && param->value == nullptr)
            throw CompilerException(kCompilerErrors[4]);
    }

    // Build default arguments tuple!
    if (def_count > 0) {
        this->unit_->Emit(vm::OpCode::MKTP, def_count, nullptr, loc);

        flags |= FunctionFlags::DEFARGS;
    } else
        this->unit_->Emit(vm::OpCode::PSHN, nullptr);
}

void Compiler::CompileFunctionDefBody(const node::Function *func, String *name) {
    ARC msg;

    msg = (ArObject *) StringFormat(kNotImplementedError[1], ARGON_RAW_STRING(name));

    this->LoadStatic((ArObject *) type_error_, &func->loc, true, true);

    this->LoadStaticAtom(kNotImplementedError[0], &func->loc, true);

    this->LoadStatic(msg.Get(), &func->loc, false, true);

    this->unit_->Emit(vm::OpCode::CALL, (unsigned char) vm::OpCodeCallMode::FASTCALL, 2, &func->loc);

    this->unit_->Emit(vm::OpCode::PANIC, &func->loc);
}

void Compiler::CompileFunctionParams(List *params, unsigned short &count, FunctionFlags &flags) {
    ARC iter;
    ARC a_param;

    if (params == nullptr)
        return;

    iter = IteratorGet((ArObject *) params, false);
    if (!iter)
        throw DatatypeException();

    while ((a_param = IteratorNext(iter.Get()))) {
        const auto *param = (node::Parameter *) a_param.Get();

        CHECK_AST_NODE(node::type_ast_parameter_, param);

        if (count == 0 && this->unit_->prev != nullptr) {
            auto pscope = this->unit_->prev->symt->type;
            if ((pscope == SymbolType::STRUCT || pscope == SymbolType::TRAIT) && StringEqual(param->id, "self"))
                flags |= FunctionFlags::METHOD;
        }

        this->IdentifierNew(param->id, &param->loc, SymbolType::VARIABLE, {}, false);

        if (param->value == nullptr)
            count++;

        if (param->node_type == node::NodeType::REST) {
            flags |= FunctionFlags::VARIADIC;
            count--;
        } else if (param->node_type == node::NodeType::KWARG) {
            flags |= FunctionFlags::KWARGS;
            count--;
        } else if (param->node_type != node::NodeType::PARAMETER)
            throw CompilerException(kCompilerErrors[1], (int) param->node_type, __FUNCTION__);
    }
}

void Compiler::CompileInfix(const node::Binary *binary) {
    CHECK_AST_NODE(node::type_ast_infix_, binary);

    this->Expression((const node::Node *) binary->left);
    this->Expression((const node::Node *) binary->right);

    switch (binary->token_type) {
        case scanner::TokenType::ARROW_RIGHT:
            this->unit_->Emit(vm::OpCode::PSHC, &binary->loc);
            break;

            // Maths
        case scanner::TokenType::PLUS:
            this->unit_->Emit(vm::OpCode::ADD, &binary->loc);
            break;
        case scanner::TokenType::MINUS:
            this->unit_->Emit(vm::OpCode::SUB, &binary->loc);
            break;
        case scanner::TokenType::ASTERISK:
            this->unit_->Emit(vm::OpCode::MUL, &binary->loc);
            break;
        case scanner::TokenType::SLASH:
            this->unit_->Emit(vm::OpCode::DIV, &binary->loc);
            break;
        case scanner::TokenType::SLASH_SLASH:
            this->unit_->Emit(vm::OpCode::IDIV, &binary->loc);
            break;
        case scanner::TokenType::PERCENT:
            this->unit_->Emit(vm::OpCode::MOD, &binary->loc);
            break;

            // SHIFT
        case scanner::TokenType::SHL:
            this->unit_->Emit(vm::OpCode::SHL, &binary->loc);
            break;
        case scanner::TokenType::SHR:
            this->unit_->Emit(vm::OpCode::SHR, &binary->loc);
            break;

            // EQUALITY
        case scanner::TokenType::EQUAL_EQUAL:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::EQ, nullptr, &binary->loc);
            break;
        case scanner::TokenType::EQUAL_STRICT:
            this->unit_->Emit(vm::OpCode::EQST, (int) CompareMode::EQ, nullptr, &binary->loc);
            break;
        case scanner::TokenType::NOT_EQUAL:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::NE, nullptr, &binary->loc);
            break;
        case scanner::TokenType::NOT_EQUAL_STRICT:
            this->unit_->Emit(vm::OpCode::EQST, (int) CompareMode::NE, nullptr, &binary->loc);
            break;

            // LOGICAL
        case scanner::TokenType::AMPERSAND:
            this->unit_->Emit(vm::OpCode::LAND, &binary->loc);
            break;
        case scanner::TokenType::PIPE:
            this->unit_->Emit(vm::OpCode::LOR, &binary->loc);
            break;
        case scanner::TokenType::CARET:
            this->unit_->Emit(vm::OpCode::LXOR, &binary->loc);
            break;

            // RELATIONAL
        case scanner::TokenType::GREATER:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::GR, nullptr, &binary->loc);
            break;
        case scanner::TokenType::GREATER_EQ:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::GRQ, nullptr, &binary->loc);
            break;
        case scanner::TokenType::LESS:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::LE, nullptr, &binary->loc);
            break;
        case scanner::TokenType::LESS_EQ:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::LEQ, nullptr, &binary->loc);
            break;
        default:
            throw CompilerException(kCompilerErrors[2], (int) binary->token_type, __FUNCTION__);
    }
}

void Compiler::CompileNullCoalescing(const node::Binary *binary) {
    CHECK_AST_NODE(node::type_ast_binary_, binary);

    this->Expression((node::Node *) binary->left);

    auto *end = BasicBlockNew();
    if (end == nullptr)
        throw DatatypeException();

    try {
        this->unit_->Emit(vm::OpCode::JNN, 0, end, &binary->loc);
        this->unit_->EmitPOP();

        this->Expression((node::Node *) binary->right);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileObjInit(const node::ObjectInit *init) {
    ARC iter;
    ARC cursor;

    CHECK_AST_NODE(node::type_ast_objinit_, init);

    this->Expression(init->left);

    if (init->values == nullptr) {
        this->unit_->Emit(vm::OpCode::INIT, (unsigned char) vm::OpCodeInitMode::POSITIONAL, 0, &init->loc);

        return;
    }

    iter = IteratorGet(init->values, false);
    if (!iter)
        throw DatatypeException();

    unsigned char items = 0;
    while ((cursor = IteratorNext(iter.Get()))) {
        const auto *node = (const node::Node *) cursor.Get();

        if (init->as_map) {
            if (items++ & 0x01) {
                this->Expression(node);
                continue;
            }

            CHECK_AST_NODE(node::type_ast_identifier_, node);

            this->LoadStatic((ArObject *) ((const node::Unary *) node)->value, &node->loc, true, true);
        } else {
            items++;

            this->Expression(node);
        }
    }

    this->unit_->Emit(vm::OpCode::INIT,
                      (unsigned char) (init->as_map ? vm::OpCodeInitMode::KWARGS : vm::OpCodeInitMode::POSITIONAL),
                      items, &init->loc);
}

void Compiler::CompilePrefix(const node::Unary *unary) {
    CHECK_AST_NODE(node::type_ast_prefix_, unary);

    this->Expression((const node::Node *) unary->value);

    switch (unary->token_type) {
        case scanner::TokenType::EXCLAMATION:
            this->unit_->Emit(vm::OpCode::NOT, &unary->loc);
            break;
        case scanner::TokenType::MINUS:
            this->unit_->Emit(vm::OpCode::NEG, &unary->loc);
            break;
        case scanner::TokenType::PLUS:
            this->unit_->Emit(vm::OpCode::POS, &unary->loc);
            break;
        case scanner::TokenType::TILDE:
            this->unit_->Emit(vm::OpCode::INV, &unary->loc);
            break;
        default:
            throw CompilerException(kCompilerErrors[2], (int) unary->token_type, __FUNCTION__);
    }
}

void Compiler::CompileSafe(const node::Unary *unary) {
    auto *val = (const node::Node *) unary->value;
    BasicBlock *end;

    CHECK_AST_NODE(node::type_ast_unary_, unary);

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        this->unit_->JBPush(nullptr, nullptr, end, JBlockType::SAFE);

        if (val->node_type == node::NodeType::ASSIGNMENT)
            this->Compile(val);
        else
            this->Expression(val);

        this->unit_->JBPop();

        this->unit_->BlockAppend(end);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }
}

void Compiler::CompileSubscr(const node::Subscript *subscr, bool dup, bool emit) {
    CHECK_AST_NODE(node::type_ast_subscript_, subscr);

    this->Expression(subscr->expression);

    if (subscr->start != nullptr)
        this->Expression(subscr->start);
    else
        this->LoadStaticNil(&subscr->loc, true);

    if (subscr->node_type == node::NodeType::SLICE) {
        if (subscr->stop != nullptr)
            this->Expression(subscr->stop);
        else
            this->LoadStaticNil(&subscr->loc, true);

        this->unit_->Emit(vm::OpCode::MKBND, &subscr->loc);
    }

    if (dup)
        this->unit_->Emit(vm::OpCode::DUP, 2, nullptr, nullptr);

    if (emit)
        this->unit_->Emit(vm::OpCode::SUBSCR, &subscr->loc);
}

void Compiler::CompileTest(const node::Binary *binary) {
    auto *cursor = binary;

    CHECK_AST_NODE(node::type_ast_infix_, binary);

    auto *end = BasicBlockNew();
    if (end == nullptr)
        throw DatatypeException();

    int deep = 0;
    while (((node::Node *) (cursor->left))->token_type == scanner::TokenType::AND
           || ((node::Node *) (cursor->left))->token_type == scanner::TokenType::OR) {
        cursor = (const node::Binary *) cursor->left;
        deep++;
    }

    try {
        this->Expression((node::Node *) cursor->left);

        do {
            if (cursor->token_type == scanner::TokenType::AND)
                this->unit_->Emit(vm::OpCode::JFOP, 0, end, &cursor->loc);
            else if (cursor->token_type == scanner::TokenType::OR)
                this->unit_->Emit(vm::OpCode::JTOP, 0, end, &cursor->loc);
            else
                throw CompilerException(kCompilerErrors[2], (int) cursor->token_type, __FUNCTION__);

            this->unit_->BlockNew();

            this->Expression((node::Node *) cursor->right);

            deep--;

            cursor = binary;
            for (int i = 0; i < deep; i++)
                cursor = (const node::Binary *) cursor->left;
        } while (deep >= 0);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileTernary(const node::Branch *branch) {
    BasicBlock *end;
    BasicBlock *orelse;

    CHECK_AST_NODE(node::type_ast_branch_, branch);

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    if ((orelse = BasicBlockNew()) == nullptr) {
        BasicBlockDel(end);

        throw DatatypeException();
    }

    try {
        this->Expression(branch->test);

        this->unit_->Emit(vm::OpCode::JF, 0, orelse, &branch->test->loc);

        this->Expression(branch->body);

        this->unit_->Emit(vm::OpCode::JMP, 0, end, nullptr);

        this->unit_->DecrementStack(1);

        this->unit_->BlockAppend(orelse);

        if (branch->orelse != nullptr)
            this->Expression(branch->orelse);
        else
            this->LoadStaticNil(&branch->loc, true);
    } catch (...) {
        BasicBlockDel(end);
        BasicBlockDel(orelse);

        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileTrap(const node::Unary *unary) {
    BasicBlock *end;

    CHECK_AST_NODE(node::type_ast_unary_, unary);

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        this->unit_->JBPush(nullptr, nullptr, end, JBlockType::TRAP);

        this->unit_->Emit(vm::OpCode::ST, 0, end, &unary->loc);

        this->Expression((const node::Node *) unary->value);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->JBPop();

    this->unit_->BlockAppend(end);

    this->unit_->Emit(vm::OpCode::POPGT, (int) this->unit_->stack.current, nullptr, nullptr);

    if (this->unit_->CheckBlock(JBlockType::TRAP))
        this->unit_->Emit(vm::OpCode::TRAP, 0, this->unit_->jblock->end, nullptr);
    else
        this->unit_->Emit(vm::OpCode::TRAP, 0, nullptr, nullptr);
}

void Compiler::CompileUpdate(const node::Unary *unary) {
    CHECK_AST_NODE(node::type_ast_update_, unary);

    this->Expression((const node::Node *) unary->value);

    this->unit_->Emit(vm::OpCode::DUP, 1, nullptr, nullptr);

    if (unary->token_type == scanner::TokenType::MINUS_MINUS)
        this->unit_->Emit(vm::OpCode::DEC, &unary->loc);
    else if (unary->token_type == scanner::TokenType::PLUS_PLUS)
        this->unit_->Emit(vm::OpCode::INC, &unary->loc);
    else
        throw CompilerException(kCompilerErrors[2], (int) unary->token_type, __FUNCTION__);

    auto *value = (const node::Unary *) unary->value;

    switch (value->node_type) {
        case node::NodeType::IDENTIFIER:
            this->StoreVariable(value);
            break;
        case node::NodeType::INDEX:
            this->CompileSubscr((const node::Subscript *) value, false, false);

            this->unit_->Emit(vm::OpCode::MTH, 2, nullptr, nullptr);
            this->unit_->Emit(vm::OpCode::STSUBSCR, &value->loc);
            break;
        case node::NodeType::SELECTOR: {
            auto code = vm::OpCode::STATTR;
            if (value->token_type == scanner::TokenType::SCOPE)
                code = vm::OpCode::STSCOPE;

            auto idx = this->CompileSelector((const node::Binary *) value, false, false);

            this->unit_->Emit(vm::OpCode::MTH, 1, nullptr, nullptr);
            this->unit_->Emit(code, idx, nullptr, &value->loc);
            break;
        }
        default:
            throw CompilerException(kCompilerErrors[1], (int) unary->node_type, __FUNCTION__);
    }
}

void Compiler::Expression(const node::Node *node) {
    switch (node->node_type) {
        case node::NodeType::AWAIT:
            CHECK_AST_NODE(node::type_ast_unary_, node);

            this->Expression((const node::Node *) ((const node::Unary *) node)->value);

            this->unit_->Emit(vm::OpCode::AWAIT, &node->loc);
            break;
        case node::NodeType::CALL:
            this->CompileCall((const node::Call *) node);
            break;
        case node::NodeType::DICT:
        case node::NodeType::LIST:
        case node::NodeType::SET:
        case node::NodeType::TUPLE:
            this->CompileDLST((const node::Unary *) node);
            break;
        case node::NodeType::ELVIS:
            this->CompileElvis((const node::Binary *) node);
            break;
        case node::NodeType::FUNCTION:
            this->CompileFunction((const node::Function *) node);
            break;
        case node::NodeType::IDENTIFIER:
            this->LoadIdentifier((const node::Unary *) node);
            break;
        case node::NodeType::IN:
        case node::NodeType::NOT_IN:
            CHECK_AST_NODE(node::type_ast_binary_, node);

            this->Expression((const node::Node *) ((const node::Binary *) node)->left);
            this->Expression((const node::Node *) ((const node::Binary *) node)->right);

            this->unit_->Emit(vm::OpCode::CNT,
                              (int) (node->node_type == node::NodeType::IN
                                     ? vm::OpCodeContainsMode::IN
                                     : vm::OpCodeContainsMode::NOT_IN),
                              nullptr,
                              &node->loc);
            break;
        case node::NodeType::INDEX:
        case node::NodeType::SLICE:
            this->CompileSubscr((const node::Subscript *) node, false, true);
            break;
        case node::NodeType::INFIX:
            if (node->token_type == scanner::TokenType::AND || node->token_type == scanner::TokenType::OR) {
                this->CompileTest((const node::Binary *) node);
                break;
            }

            this->CompileInfix((const node::Binary *) node);
            break;
        case node::NodeType::OBJ_INIT:
            this->CompileObjInit((const node::ObjectInit *) node);
            break;
        case node::NodeType::LITERAL:
            this->LoadStatic((const node::Unary *) node, true, true);
            break;
        case node::NodeType::NULL_COALESCING:
            this->CompileNullCoalescing((const node::Binary *) node);
            break;
        case node::NodeType::PREFIX:
            this->CompilePrefix((const node::Unary *) node);
            break;
        case node::NodeType::TERNARY:
            this->CompileTernary((const node::Branch *) node);
            break;
        case node::NodeType::TRAP:
            this->CompileTrap((const node::Unary *) node);
            break;
        case node::NodeType::SAFE_EXPR:
            this->CompileSafe((const node::Unary *) node);
            break;
        case node::NodeType::SELECTOR:
            this->CompileSelector((const node::Binary *) node, false, true);
            break;
        case node::NodeType::UPDATE:
            this->CompileUpdate((const node::Unary *) node);
            break;
        default:
            throw CompilerException(kCompilerErrors[1], (int) node->node_type, __FUNCTION__);
    }
}

void Compiler::StoreVariable(String *id, const scanner::Loc *loc) {
    vm::OpCode code = vm::OpCode::STGBL;

    if (StringEqual(id, (const char *) "_")) {
        this->unit_->Emit(vm::OpCode::POP, nullptr);

        return;
    }

    auto *sym = this->IdentifierLookupOrCreate(id, SymbolType::VARIABLE);

    if (sym->declared && (this->unit_->symt->type == SymbolType::FUNC || sym->nested > 0))
        code = vm::OpCode::STLC;
    else if (sym->free)
        code = vm::OpCode::STENC;

    auto sym_id = sym->id;

    Release(sym);

    this->unit_->Emit(code, sym_id, nullptr, loc);
}

void Compiler::StoreVariable(const node::Unary *identifier) {
    CHECK_AST_NODE(node::type_ast_identifier_, identifier);

    return this->StoreVariable((String *) identifier->value, &identifier->loc);
}

// *********************************************************************************************************************
// PRIVATE
// *********************************************************************************************************************

void Compiler::EnterScope(String *name, SymbolType type) {
    auto *unit = TranslationUnitNew(this->unit_, name, type);
    if (unit == nullptr)
        throw DatatypeException();

    this->unit_ = unit;
}

void Compiler::ExitScope() {
    this->unit_ = TranslationUnitDel(this->unit_);
}

// *********************************************************************************************************************
// PUBLIC
// *********************************************************************************************************************

Code *Compiler::Compile(node::Module *mod) {
    ARC decl_iter;
    ArObject *decl;

    // Initialize global_statics
    if (this->static_globals_ == nullptr) {
        this->static_globals_ = DictNew();

        if (this->static_globals_ == nullptr)
            return nullptr;
    }

    try {
        decl_iter = IteratorGet((ArObject *) mod->statements, false);
        if (!decl_iter)
            throw DatatypeException();

        // Let's start creating a new context
        this->EnterScope(mod->filename, SymbolType::MODULE);

        while ((decl = IteratorNext(decl_iter.Get())) != nullptr) {
            this->Compile((node::Node *) decl);

            Release(&decl);
        }

        Release(decl);

        this->ExitScope();
    } catch (CompilerException &e) {
        assert(false);
    }

    return nullptr;
}
