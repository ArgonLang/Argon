// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cctype>

#include <argon/vm/opcode.h>

#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/nil.h>

#include <argon/lang/scanner/token.h>

#include <argon/lang/compilererr.h>
#include <argon/lang/compiler.h>
#include <argon/vm/datatype/atom.h>

using namespace argon::lang;
using namespace argon::lang::parser;
using namespace argon::vm::datatype;

#define CHECK_AST_NODE(node, node_type, message)    \
    do {                                            \
    if (!AR_TYPEOF(node, node_type))                \
        throw CompilerException(message);           \
    } while(false)                                  \


int Compiler::CompileSelector(const parser::Binary *selector, bool dup, bool emit) {
    const auto *cursor = selector;
    vm::OpCode code;
    int deep;
    int idx;

    deep = 0;
    while (cursor->left->node_type == parser::NodeType::SELECTOR) {
        cursor = (const parser::Binary *) cursor->left;
        deep++;
    }

    this->Expression(cursor->left);

    do {
        if (cursor->token_type == scanner::TokenType::SCOPE)
            code = vm::OpCode::LDSCOPE;
        else if (cursor->token_type == scanner::TokenType::DOT)
            code = vm::OpCode::LDATTR;
        else if (cursor->token_type == scanner::TokenType::QUESTION_DOT) {
            code = vm::OpCode::LDATTR;
            this->unit_->Emit(vm::OpCode::JNIL, this->unit_->jstack->end, &cursor->loc);
        } else
            throw CompilerException("unexpected TokenType in selector expression");

        idx = this->LoadStatic(((const Unary *) cursor->right)->value, true, false);

        if (dup && deep == 0) {
            this->unit_->Emit(vm::OpCode::DUP, 1, nullptr, nullptr);

            this->unit_->IncrementStack(1);
        }

        if (deep > 0 || emit)
            this->unit_->Emit(code, idx, nullptr, &cursor->loc);

        deep--;
        cursor = selector;
        for (int i = 0; i < deep; i++)
            cursor = (const parser::Binary *) selector->left;
    } while (deep >= 0);

    return idx;
}

int Compiler::LoadStatic(ArObject *value, bool store, bool emit) {
    ArObject *tmp;
    Integer *index;
    IntegerUnderlying idx = -1;

    IncRef(value);

    if (store) {
        // Check if value are already present in TranslationUnit
        if ((tmp = DictLookup(this->unit_->statics_map, value)) == nullptr) {
            // Value not found in the current TranslationUnit, trying in global_statics
            if ((tmp = DictLookup(this->statics_globals_, value)) != nullptr) {
                // Recover already existing object and discard the actual one
                Release(value);
                value = tmp;
            } else if (!DictInsert(this->statics_globals_, value, value)) {
                Release(value);
                throw DatatypeException();
            }

            if ((index = UIntNew(this->unit_->statics->length)) == nullptr) {
                Release(value);
                throw DatatypeException();
            }

            if (!DictInsert(this->unit_->statics_map, value, (ArObject *) index)) {
                Release(value);
                Release(index);
                throw DatatypeException();
            }

            Release((ArObject **) &index);
        } else {
            idx = ((Integer *) tmp)->sint;
            Release(tmp);
        }
    }

    if (!store || idx == -1) {
        idx = (IntegerUnderlying) this->unit_->statics->length;
        if (!ListAppend(this->unit_->statics, value)) {
            Release(value);
            throw DatatypeException();
        }
    }

    Release(value);

    if (emit)
        this->unit_->Emit(vm::OpCode::LSTATIC, (int) idx, nullptr, nullptr);

    return (int) idx;
}

String *Compiler::MakeFname() {
    String *name;

    if (this->unit_->name != nullptr)
        name = StringFormat("%s$%d", ARGON_RAW_STRING(this->unit_->name), this->unit_->anon_count_++);
    else
        name = StringFormat("$%d", this->unit_->anon_count_++);

    if (name == nullptr)
        throw DatatypeException();

    return name;
}

String *Compiler::MakeImportName(const vm::datatype::String *mod_name) {
    const unsigned char *end = ARGON_RAW_STRING(mod_name) + ARGON_RAW_STRING_LENGTH(mod_name);
    unsigned int idx = 0;

    while (idx < ARGON_RAW_STRING_LENGTH(mod_name) && std::isalnum(*((end - idx) - 1)))
        idx++;

    if (!std::isalpha(*(end - idx))) {
        ErrorFormat(kCompilerError[0], "alias required for: %s", ARGON_RAW_STRING(mod_name));
        throw DatatypeException();
    }

    auto *ret = StringNew((const char *) end - idx, idx);
    if (ret == nullptr)
        throw DatatypeException();

    return ret;
}

String *Compiler::MakeQname(String *name) {
    const char *sep = "%s.%s";

    String *qname;

    assert(name != nullptr);

    if (this->unit_->qname != nullptr) {
        if (this->unit_->symt->type != SymbolType::MODULE)
            sep = "%s::%s";

        if ((qname = StringFormat(sep, ARGON_RAW_STRING(this->unit_->qname), ARGON_RAW_STRING(name))) == nullptr)
            throw DatatypeException();

        return qname;
    }

    return IncRef(name);
}

SymbolT *Compiler::IdentifierLookupOrCreate(String *name, SymbolType type) {
    List *dst = this->unit_->names;

    SymbolT *sym;
    if ((sym = SymbolLookup(this->unit_->symt, name)) == nullptr) {
        if ((sym = SymbolInsert(this->unit_->symt, name, type)) == nullptr)
            throw DatatypeException();

        if (this->unit_->IsFreeVar(name)) {
            dst = this->unit_->enclosed;
            sym->free = true;
        }

        sym->id = (unsigned int) dst->length;

        if (!ListAppend(dst, (ArObject *) name)) {
            Release(sym);
            throw DatatypeException();
        }
    }

    return sym;
}

void Compiler::Binary(const parser::Binary *binary) {
    CHECK_AST_NODE(binary, type_ast_binary_, "Compiler::Binary: invalid AST node");

    this->Expression(binary->left);
    this->Expression(binary->right);

    switch (binary->token_type) {
        case lang::scanner::TokenType::PLUS:
            this->unit_->Emit(vm::OpCode::ADD, &binary->loc);
            break;
        case lang::scanner::TokenType::MINUS:
            this->unit_->Emit(vm::OpCode::SUB, &binary->loc);
            break;
        case lang::scanner::TokenType::ASTERISK:
            this->unit_->Emit(vm::OpCode::MUL, &binary->loc);
            break;
        case lang::scanner::TokenType::SLASH:
            this->unit_->Emit(vm::OpCode::DIV, &binary->loc);
            break;
        case lang::scanner::TokenType::SLASH_SLASH:
            this->unit_->Emit(vm::OpCode::IDIV, &binary->loc);
            break;
        case lang::scanner::TokenType::PERCENT:
            this->unit_->Emit(vm::OpCode::MOD, &binary->loc);
            break;

            // SHIFT
        case lang::scanner::TokenType::SHL:
            this->unit_->Emit(vm::OpCode::SHL, &binary->loc);
            break;
        case lang::scanner::TokenType::SHR:
            this->unit_->Emit(vm::OpCode::SHR, &binary->loc);
            break;

            // EQUALITY
        case lang::scanner::TokenType::EQUAL_EQUAL:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::EQ, nullptr, &binary->loc);
            break;
        case lang::scanner::TokenType::EQUAL_STRICT:
            this->unit_->Emit(vm::OpCode::EQST, (int) CompareMode::EQ, nullptr, &binary->loc);
            break;
        case lang::scanner::TokenType::NOT_EQUAL:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::NE, nullptr, &binary->loc);
            break;
        case lang::scanner::TokenType::NOT_EQUAL_STRICT:
            this->unit_->Emit(vm::OpCode::EQST, (int) CompareMode::NE, nullptr, &binary->loc);
            break;

            // LOGICAL
        case lang::scanner::TokenType::AMPERSAND:
            this->unit_->Emit(vm::OpCode::LAND, &binary->loc);
            break;
        case lang::scanner::TokenType::PIPE:
            this->unit_->Emit(vm::OpCode::LOR, &binary->loc);
            break;
        case lang::scanner::TokenType::CARET:
            this->unit_->Emit(vm::OpCode::LXOR, &binary->loc);
            break;

            // RELATIONAL
        case lang::scanner::TokenType::GREATER:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::GR, nullptr, &binary->loc);
            break;
        case lang::scanner::TokenType::GREATER_EQ:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::GRQ, nullptr, &binary->loc);
            break;
        case lang::scanner::TokenType::LESS:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::LE, nullptr, &binary->loc);
            break;
        case lang::scanner::TokenType::LESS_EQ:
            this->unit_->Emit(vm::OpCode::CMP, (int) CompareMode::LEQ, nullptr, &binary->loc);
            break;
        default:
            throw CompilerException("failed to map token to binary operation(invalid token found)");
    }
}

void Compiler::Compile(const Node *node) {
    switch (node->node_type) {
        case NodeType::ASSERT:
            this->CompileAssertion((const parser::Binary *) node);
            break;
        case NodeType::ASSIGNMENT:
            this->CompileAssignment((const parser::Binary *) node);
            break;
        case NodeType::BLOCK:
            this->CompileBlock(node, true);
            break;
        case NodeType::CALL:
            this->CompileCall((const parser::Call *) node);
            break;
        case NodeType::DECLARATION:
            this->CompileDeclaration((const Assignment *) node);
            break;
        case parser::NodeType::EXPRESSION:
            this->Expression((Node *) ((const Unary *) node)->value);
            this->unit_->Emit(vm::OpCode::POP, nullptr);
            break;
        case NodeType::FOR:
            this->CompileForLoop((const Loop *) node);
            break;
        case NodeType::FOREACH:
            this->CompileForEach((const Loop *) node);
            break;
        case NodeType::FUNC:
            this->CompileFunction((const parser::Function *) node);
            break;
        case NodeType::JUMP:
            this->CompileJump((const parser::Unary *) node);
            break;
        case NodeType::IMPORT:
            this->CompileImport((const parser::Import *) node);
            break;
        case NodeType::LABEL:
            this->unit_->JBNew((String *) ((const Unary *) ((const parser::Binary *) node)->left)->value);
            this->Compile(((const parser::Binary *) node)->right);
            break;
        case NodeType::LOOP:
            this->CompileLoop((const Loop *) node);
            break;
        case NodeType::PANIC:
            this->Expression((const Node *) ((const Unary *) node)->value);
            this->unit_->Emit(vm::OpCode::PANIC, &node->loc);
            break;
        case NodeType::RETURN:
            if (((const Unary *) node)->value != nullptr)
                this->Expression((const Node *) ((const Unary *) node)->value);
            else
                this->LoadStatic((ArObject *) Nil, true, true);

            this->unit_->Emit(vm::OpCode::RET, &node->loc);
            break;
        case NodeType::SAFE_EXPR:
            this->CompileSafe((const Unary *) node);
            break;
        case NodeType::STRUCT:
        case NodeType::TRAIT:
            this->CompileConstruct((const parser::Construct *) node);
            break;
        case NodeType::SWITCH:
            this->CompileSwitch((const Test *) node);
            break;
        case NodeType::IF:
            this->CompileIf((const Test *) node);
            break;
        case NodeType::YIELD:
            if (this->unit_->symt->type != SymbolType::FUNC && this->unit_->symt->type != SymbolType::GENERATOR)
                throw CompilerException("yield outside function definition");

            this->unit_->symt->type = SymbolType::GENERATOR;

            this->Expression((const Node *) ((const Unary *) node)->value);
            this->unit_->Emit(vm::OpCode::YLD, &node->loc);
            break;
        default:
            assert(false);
    }
}

void Compiler::CompileAssertion(const parser::Binary *binary) {
    BasicBlock *end;
    unsigned short args = 1;

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        this->Expression(binary->left);

        this->unit_->Emit(vm::OpCode::JT, end, nullptr);

        if (!this->unit_->BlockNew())
            throw DatatypeException();

        // Assertion failed:
        this->LoadStatic((ArObject *) type_error_, true, true);

        this->PushAtom(kAssertionError[0], true);

        if (binary->right == nullptr) {
            auto *empty = StringIntern("");

            this->LoadStatic((ArObject *) empty, true, true);

            Release(empty);
        } else
            this->Expression(binary->right);

        args++;

        this->unit_->Emit(vm::OpCode::CALL, (unsigned char) vm::OpCodeCallMode::FASTCALL, args, &binary->loc);

        this->unit_->DecrementStack(args);

        this->unit_->Emit(vm::OpCode::PANIC, &binary->loc);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileAssignment(const parser::Binary *assign) {
    if (assign->token_type != scanner::TokenType::EQUAL) {
        this->CompileAugAssignment(assign);
        return;
    }

    this->Expression(assign->right);

    if (assign->left->node_type == NodeType::IDENTIFIER)
        this->StoreVariable((String *) ((const parser::Unary *) assign->left)->value, &assign->loc);
    else if (assign->left->node_type == NodeType::SELECTOR) {
        auto idx = this->CompileSelector((const parser::Binary *) assign->left, false, false);

        if (assign->left->token_type == scanner::TokenType::SCOPE) {
            this->unit_->Emit(vm::OpCode::STSCOPE, idx, nullptr, &assign->loc);
            return;
        }

        this->unit_->Emit(vm::OpCode::STATTR, idx, nullptr, &assign->loc);
    } else if (assign->left->node_type == parser::NodeType::INDEX ||
               assign->left->node_type == parser::NodeType::SLICE) {
        this->CompileSubscr((const Subscript *) assign->left, false, false);

        this->unit_->Emit(vm::OpCode::STSUBSCR, &assign->loc);
    } else if (assign->left->node_type == parser::NodeType::TUPLE)
        this->CompileUnpack((List *) ((const Unary *) assign->left)->value, &assign->loc);
}

void Compiler::CompileAugAssignment(const parser::Binary *assign) {
    const char *assign_op_token = "+=";

#define COMPILE_OP()                        \
    this->Expression(assign->right);        \
    this->unit_->Emit(opcode, &assign->loc)

    vm::OpCode opcode;
    int idx;

    // Select opcode
    switch (assign->token_type) {
        case scanner::TokenType::ASSIGN_ADD:
            opcode = vm::OpCode::IPADD;
            break;
        case scanner::TokenType::ASSIGN_SUB:
            opcode = vm::OpCode::IPSUB;
            assign_op_token = "-=";
            break;
        default:
            throw CompilerException("invalid token for CompileAugAssignment");
    }

    switch (assign->left->node_type) {
        case NodeType::IDENTIFIER:
            this->LoadIdentifier((String *) ((const Unary *) assign->left)->value);

            COMPILE_OP();

            this->StoreVariable((String *) ((const Unary *) assign->left)->value, &assign->loc);
            break;
        case NodeType::SELECTOR:
            idx = this->CompileSelector((const parser::Binary *) assign->left, true, true);

            COMPILE_OP();

            this->unit_->Emit(vm::OpCode::PBHEAD, 1, nullptr, nullptr);

            if (assign->left->token_type == scanner::TokenType::SCOPE) {
                this->unit_->Emit(vm::OpCode::STSCOPE, idx, nullptr, &assign->loc);
                break;
            }

            this->unit_->Emit(vm::OpCode::STATTR, idx, nullptr, &assign->loc);
            break;
        case NodeType::INDEX:
        case NodeType::SLICE:
            this->CompileSubscr((const Subscript *) assign->left, true, true);

            COMPILE_OP();

            this->unit_->Emit(vm::OpCode::PBHEAD, 3, nullptr, nullptr);
            this->unit_->Emit(vm::OpCode::STSUBSCR, &assign->loc);
            break;
        default:
            ErrorFormat(kCompilerError[0], "%s operator cannot be applied to the left expression", assign_op_token);
            throw DatatypeException();
    }

#undef COMPILE_OP
}

void Compiler::CompileBlock(const parser::Node *node, bool sub) {
    ARC iter;
    ARC stmt;

    iter = IteratorGet(((const Unary *) node)->value, false);
    if (!iter)
        throw DatatypeException();

    if (sub && !SymbolNewSub(this->unit_->symt))
        throw DatatypeException();

    while ((stmt = IteratorNext(iter.Get())))
        this->Compile((const Node *) stmt.Get());

    if (sub)
        SymbolExitSub(this->unit_->symt);
}

void Compiler::CompileCall(const parser::Call *call) {
    ARC iter;
    ARC param;

    vm::OpCode code = vm::OpCode::CALL;
    vm::OpCodeCallMode mode = vm::OpCodeCallMode::FASTCALL;
    int args = 0;

    if (call->left->node_type == parser::NodeType::SELECTOR &&
        call->left->token_type != scanner::TokenType::SCOPE) {
        auto idx = this->CompileSelector((const parser::Binary *) call->left, false, false);

        this->unit_->Emit(vm::OpCode::LDMETH, idx, nullptr, nullptr);

        args = 1;
    } else
        this->Expression(call->left);

    if (call->args != nullptr)
        this->CompileCallPositional(call->args, args, mode);

    if (call->kwargs != nullptr)
        this->CompileCallKwArgs(call->kwargs, args, mode);

    if (call->token_type == scanner::TokenType::KW_DEFER)
        code = vm::OpCode::DFR;
    else if (call->token_type == scanner::TokenType::KW_SPAWN)
        code = vm::OpCode::SPW;

    this->unit_->DecrementStack(args);

    this->unit_->Emit(code, (unsigned char) mode, args, &call->loc);
}

void Compiler::CompileCallKwArgs(vm::datatype::ArObject *args, int &args_count, vm::OpCodeCallMode &mode) {
    ARC iter;
    ARC param;
    ARC keys;

    int items = 0;

    iter = IteratorGet(args, false);
    if (!iter)
        throw DatatypeException();

    while ((param = IteratorNext(iter.Get()))) {
        const auto *tmp = (const Unary *) param.Get();

        if ((items & 1) == 0)
            this->LoadStatic(tmp->value, false, true);
        else
            this->Expression((const Node *) tmp);

        items++;
    }

    this->unit_->DecrementStack(items);

    this->unit_->Emit(vm::OpCode::MKDT, items, nullptr, nullptr);

    mode |= vm::OpCodeCallMode::KW_PARAMS;

    if (ENUMBITMASK_ISTRUE(mode, vm::OpCodeCallMode::REST_PARAMS)) {
        this->unit_->Emit(vm::OpCode::PLT, nullptr);
        return;
    }

    args_count++;
}

void Compiler::CompileCallPositional(vm::datatype::ArObject *args, int &args_count, vm::OpCodeCallMode &mode) {
    ARC iter;
    ARC param;

    iter = IteratorGet(args, false);
    if (!iter)
        throw DatatypeException();

    while ((param = IteratorNext(iter.Get()))) {
        const auto *tmp = (const Node *) param.Get();

        if (tmp->node_type == parser::NodeType::ELLIPSIS) {
            if (ENUMBITMASK_ISFALSE(mode, vm::OpCodeCallMode::REST_PARAMS)) {
                this->unit_->DecrementStack(args_count);

                this->unit_->Emit(vm::OpCode::MKLT, args_count, nullptr, nullptr);
            }

            this->Expression((const Node *) ((const Unary *) tmp)->value);

            this->unit_->Emit(vm::OpCode::EXTD, nullptr);

            mode |= vm::OpCodeCallMode::REST_PARAMS;
        } else {
            this->Expression(tmp);

            if (ENUMBITMASK_ISTRUE(mode, vm::OpCodeCallMode::REST_PARAMS))
                this->unit_->Emit(vm::OpCode::PLT, nullptr);
        }

        args_count++;
    }

    if (ENUMBITMASK_ISTRUE(mode, vm::OpCodeCallMode::REST_PARAMS))
        args_count = 1;
}

void Compiler::CompileConstruct(const parser::Construct *construct) {
    vm::OpCode opcode = vm::OpCode::MKSTRUCT;
    SymbolType stype = SymbolType::STRUCT;
    AttributeFlag aflags = AttributeFlag::CONST;
    int impls = 0;

    ARC qname;
    ARC doc;

    if (construct->node_type == NodeType::TRAIT) {
        stype = SymbolType::TRAIT;
        opcode = vm::OpCode::MKTRAIT;
    }

    qname = (ArObject *) this->MakeQname(construct->name);

    doc = (ArObject *) IncRef(construct->doc);
    if (!doc)
        doc = (ArObject *) StringIntern("");

    this->TUScopeEnter(construct->name, stype);

    this->LoadStatic((ArObject *) construct->name, true, true);

    this->LoadStatic(qname.Get(), false, true);

    this->LoadStatic(doc.Get(), false, true);

    this->unit_->Emit(vm::OpCode::MKNS, nullptr);

    this->CompileBlock(construct->body, false);

    if (construct->impls != nullptr) {
        ARC iter;
        ARC tmp;

        iter = IteratorGet((ArObject *) construct->impls, false);
        if (!iter)
            throw DatatypeException();

        while ((tmp = IteratorNext(iter.Get()))) {
            this->Expression((const Node *) tmp.Get());
            impls++;
        }
    }

    this->unit_->DecrementStack(impls);

    this->unit_->Emit(opcode, impls, nullptr, &construct->loc);

    this->unit_->DecrementStack(1);

    this->TUScopeExit();

    if (construct->pub)
        aflags |= AttributeFlag::PUBLIC;

    this->unit_->IncrementStack(1);

    this->IdentifierNew(construct->name, stype, aflags, true);
}

void Compiler::CompileContains(const parser::Binary *contains) {
    CHECK_AST_NODE(contains, type_ast_binary_, "Compiler::CompileContains: invalid AST node");

    this->Expression(contains->left);
    this->Expression(contains->right);

    this->unit_->Emit(vm::OpCode::CNT, &contains->loc);
}

void Compiler::CompileDeclaration(const parser::Assignment *declaration) {
    SymbolType stype = SymbolType::VARIABLE;
    AttributeFlag aflags{};

    if (declaration->constant) {
        stype = SymbolType::CONSTANT;
        aflags = AttributeFlag::CONST;
    }

    if (declaration->pub)
        aflags |= AttributeFlag::PUBLIC;

    if (declaration->weak) {
        if (declaration->constant)
            throw CompilerException("weak modifier cannot be used with a constant declaration");

        aflags |= AttributeFlag::WEAK;
    }

    if (!declaration->multi) {
        if (declaration->value == nullptr) {
            if (declaration->constant)
                throw CompilerException("defining a constant requires a value");

            this->LoadStatic((ArObject *) Nil, true, true);
        } else
            this->Expression((const Node *) declaration->value);

        this->IdentifierNew((String *) ((const Unary *) declaration->name)->value, stype, aflags, true);

        return;
    }

    ARC iter;
    ARC tmp;

    Instr *unpack = nullptr;

    if (declaration->value != nullptr) {
        this->Expression((const Node *) declaration->value);

        this->unit_->Emit(vm::OpCode::UNPACK, 0, nullptr, &declaration->loc);

        unpack = this->unit_->bb.cur->instr.tail;
    }

    iter = IteratorGet(declaration->name, false);
    if (!iter)
        throw DatatypeException();

    unsigned short count = 0;
    while ((tmp = IteratorNext(iter.Get()))) {
        if (declaration->value == nullptr)
            this->LoadStatic((ArObject *) Nil, true, true);
        else
            this->unit_->IncrementStack(1);

        this->IdentifierNew((String *) ((const Unary *) tmp.Get())->value, stype, aflags, true);

        count++;
    }

    if (declaration->value != nullptr) {
        this->unit_->IncrementRequiredStack(count);
        unpack->oparg = count;
    }
}

void Compiler::CompileElvis(const parser::Binary *binary) {
    BasicBlock *end;

    this->Expression(binary->left);

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        this->unit_->Emit(vm::OpCode::JTOP, end, nullptr);

        this->Expression(binary->right);
    } catch (const std::exception &) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileForEach(const parser::Loop *loop) {
    BasicBlock *end = nullptr;
    BasicBlock *begin;
    JBlock *jb;

    if (!SymbolNewSub(this->unit_->symt))
        throw DatatypeException();

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        this->Expression(loop->test);

        this->unit_->Emit(vm::OpCode::LDITER, nullptr);

        if (!this->unit_->BlockNew())
            throw DatatypeException();

        begin = this->unit_->bb.cur;

        jb = this->unit_->JBNew(begin, end, 1);

        this->unit_->Emit(vm::OpCode::NXT, nullptr);
        this->unit_->Emit(vm::OpCode::JEX, end, nullptr);

        if (!this->unit_->BlockNew())
            throw DatatypeException();

        if (loop->init->node_type == NodeType::IDENTIFIER)
            this->StoreVariable((String *) ((const Unary *) loop->init)->value, &loop->init->loc);
        else if (loop->init->node_type == NodeType::TUPLE)
            this->CompileUnpack((List *) ((const Unary *) loop->init)->value, &loop->init->loc);

        this->Compile(loop->body);

        this->unit_->Emit(vm::OpCode::JMP, begin, nullptr);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    SymbolExitSub(this->unit_->symt);

    this->unit_->JBPop(jb);
    this->unit_->BlockAppend(end);

    this->unit_->DecrementStack(1); // NJE remove iterator from eval stack
}

void Compiler::CompileForLoop(const parser::Loop *loop) {
    BasicBlock *begin;
    BasicBlock *end;
    JBlock *jb;

    if (!SymbolNewSub(this->unit_->symt))
        throw DatatypeException();

    if (loop->init != nullptr)
        this->Compile(loop->init);

    if (!this->unit_->BlockNew())
        throw DatatypeException();

    begin = this->unit_->bb.cur;

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        jb = this->unit_->JBNew(begin, end, 0);

        // Compile test
        this->Expression(loop->test);

        this->unit_->Emit(vm::OpCode::JF, end, nullptr);

        this->unit_->BlockNew();

        // Compile body
        this->CompileBlock(loop->body, false);

        // Compile Inc
        if (loop->inc != nullptr)
            this->CompileForLoopInc(loop->inc);

        this->unit_->Emit(vm::OpCode::JMP, begin, nullptr);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    SymbolExitSub(this->unit_->symt);

    this->unit_->JBPop(jb);
    this->unit_->BlockAppend(end);
}

void Compiler::CompileForLoopInc(const parser::Node *node) {
    switch (node->node_type) {
        case NodeType::ASSIGNMENT:
            this->CompileAssignment((const parser::Binary *) node);
            break;
        case NodeType::CALL:
            this->CompileCall((const parser::Call *) node);
            break;
        case NodeType::UPDATE:
            this->CompileUpdate((const Unary *) node);
            this->unit_->Emit(vm::OpCode::POP, nullptr);
            break;
        default:
            throw CompilerException("invalid AST node for CompileForLoopInc");
    }
}

void Compiler::CompileFunction(const parser::Function *func) {
    ARC code;
    ARC fname;
    ARC qname;

    FunctionFlags flags{};

    unsigned short p_count = 0;

    fname = (ArObject *) func->name;
    if (func->name == nullptr)
        fname = (ArObject *) this->MakeFname();

    qname = (ArObject *) this->MakeQname((String *) fname.Get());

    this->TUScopeEnter((String *) fname.Get(), SymbolType::FUNC);

    this->CompileFunctionParams(func->params, p_count, flags);

    if (func->body != nullptr) {
        this->CompileBlock(func->body, true);
    } else
        this->CompileFunctionDefaultBody(func, (String *) fname.Get());

    // If the function is empty or the last statement is not return,
    // forcefully enter return as last statement
    if (this->unit_->bb.cur->instr.tail == nullptr ||
        this->unit_->bb.cur->instr.tail->opcode != (unsigned char) vm::OpCode::RET) {
        if (this->unit_->symt->type == SymbolType::GENERATOR)
            this->PushAtom("stop", true);
        else
            this->LoadStatic((ArObject *) Nil, true, true);

        this->unit_->Emit(vm::OpCode::RET, &func->loc);
    }

    // Update function flags if is a generator
    if (this->unit_->symt->type == SymbolType::GENERATOR)
        flags |= FunctionFlags::GENERATOR;

    code = (ArObject *) this->unit_->Assemble(func->doc);

    this->TUScopeExit();

    this->LoadStatic(fname.Get(), true, true);

    this->LoadStatic(qname.Get(), false, true);

    this->LoadStatic(code.Get(), false, true);

    // Load closure
    const auto *fn_code = (const Code *) code.Get();

    if (fn_code->enclosed->length > 0) {
        for (ArSize i = 0; i < fn_code->enclosed->length; i++)
            this->LoadIdentifier((String *) fn_code->enclosed->objects[i]);

        this->unit_->DecrementStack((int) fn_code->enclosed->length);

        this->unit_->Emit(vm::OpCode::MKLT, (int) fn_code->enclosed->length, nullptr, nullptr);

        this->unit_->DecrementStack(1);

        flags |= FunctionFlags::CLOSURE;
    }

    if (func->async)
        flags |= FunctionFlags::ASYNC;

    this->unit_->Emit(vm::OpCode::MKFN, (unsigned char) flags, p_count, &func->loc);

    if (func->name != nullptr) {
        auto aflags = AttributeFlag::CONST;

        if (func->pub)
            aflags |= AttributeFlag::PUBLIC;

        this->IdentifierNew(func->name, SymbolType::FUNC, aflags, true);
    }
}

void Compiler::CompileFunctionDefaultBody(const parser::Function *func, String *fname) {
    ARC msg;

    msg = (ArObject *) StringFormat(kNotImplementedError[1], ARGON_RAW_STRING(fname));

    this->LoadStatic((ArObject *) type_error_, true, true);

    this->PushAtom(kNotImplementedError[0], true);

    this->LoadStatic(msg.Get(), false, true);

    this->unit_->Emit(vm::OpCode::CALL, (unsigned char) vm::OpCodeCallMode::FASTCALL, 2, &func->loc);

    this->unit_->DecrementStack(2);

    this->unit_->Emit(vm::OpCode::PANIC, &func->loc);
}

void Compiler::CompileFunctionParams(vm::datatype::List *params, unsigned short &p_count, FunctionFlags &flags) {
    ARC iter;
    ARC param;

    if (params == nullptr)
        return;

    iter = IteratorGet((ArObject *) params, false);
    if (!iter)
        throw DatatypeException();

    while ((param = IteratorNext(iter.Get()))) {
        const auto *p = (Node *) param.Get();

        CHECK_AST_NODE(p, type_ast_unary_,
                       "Compiler::CompileFunctionParams: expects a unary node as an element in the parameter list");

        if (p_count == 0 && this->unit_->prev != nullptr) {
            auto pscope = this->unit_->prev->symt->type;

            if ((pscope == SymbolType::STRUCT || pscope == SymbolType::TRAIT) &&
                StringEqual((const String *) ((const Unary *) p)->value, "self"))
                flags |= FunctionFlags::METHOD;
        }

        this->IdentifierNew((String *) ((const Unary *) p)->value, SymbolType::VARIABLE, {}, false);

        p_count++;

        if (p->node_type == parser::NodeType::REST) {
            flags |= FunctionFlags::VARIADIC;
            p_count--;
        } else if (p->node_type == parser::NodeType::KWARG) {
            flags |= FunctionFlags::KWARGS;
            p_count--;
        }
    }
}

void Compiler::CompileIf(const parser::Test *test) {
    BasicBlock *orelse;
    BasicBlock *end;

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    orelse = end;

    try {
        this->Expression(test->test);

        this->unit_->Emit(vm::OpCode::JF, orelse, nullptr);

        this->unit_->BlockNew();

        this->CompileBlock(test->body, true);

        if (test->orelse != nullptr) {
            if ((end = BasicBlockNew()) == nullptr)
                throw DatatypeException();

            this->unit_->Emit(vm::OpCode::JMP, end, nullptr);

            this->unit_->BlockAppend(orelse);
            orelse = nullptr; // Avoid releasing it in case of an exception.

            this->Compile(test->orelse);
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

void Compiler::CompileImport(const parser::Import *import) {
    ARC iter;
    ARC tmp;

    if (import->mod != nullptr) {
        auto idx = this->LoadStatic(((const Unary *) import->mod)->value, true, false);
        this->unit_->Emit(vm::OpCode::IMPMOD, idx, nullptr, &import->loc);
    }

    if (import->names == nullptr) {
        this->unit_->Emit(vm::OpCode::IMPALL, &import->loc);
        return;
    }

    iter = IteratorGet(import->names, false);
    if (!iter)
        throw DatatypeException();

    while ((tmp = IteratorNext(iter.Get())))
        this->CompileImportAlias((const parser::Binary *) tmp.Get(), import->mod != nullptr);

    if (import->mod != nullptr)
        this->unit_->Emit(vm::OpCode::POP, nullptr);
}

void Compiler::CompileImportAlias(const parser::Binary *alias, bool impfrm) {
    vm::OpCode code = vm::OpCode::IMPMOD;
    int idx;

    ARC name;

    idx = this->LoadStatic(((const Unary *) alias->left)->value, true, false);

    if (impfrm)
        code = vm::OpCode::IMPFRM;

    this->unit_->Emit(code, idx, nullptr, &alias->loc);

    if (alias->right != nullptr)
        name = (ArObject *) ((const Unary *) alias->right)->value;

    if (!name) {
        if (impfrm) {
            name = ((const Unary *) alias->left)->value;
        } else
            name = (ArObject *) this->MakeImportName((String *) ((const Unary *) alias->left)->value);
    }

    this->IdentifierNew((String *) name.Get(), SymbolType::CONSTANT, AttributeFlag::CONST, true);
}

void Compiler::CompileInit(const parser::Initialization *init) {
    vm::OpCodeInitMode mode = init->as_map ? vm::OpCodeInitMode::KWARGS : vm::OpCodeInitMode::POSITIONAL;
    ARC iter;
    ARC tmp;

    this->Expression(init->left);

    if (init->values == nullptr) {
        this->unit_->DecrementStack(1); //  init->left
        this->unit_->Emit(vm::OpCode::INIT, (unsigned char) vm::OpCodeInitMode::POSITIONAL, 0, &init->loc);
        return;
    }

    iter = IteratorGet(init->values, false);
    if (!iter)
        throw DatatypeException();

    unsigned char items = 0;
    while ((tmp = IteratorNext(iter.Get()))) {
        const auto *node = (const Node *) tmp.Get();

        if (init->as_map) {
            if (items++ & 0x01) {
                this->Expression(node);
                continue;
            }

            CHECK_AST_NODE(node, type_ast_unary_,
                           "Compiler::CompileInit: expects a unary node as a key in the initialization list");

            this->LoadStatic(((const Unary *) node)->value, true, true);
            continue;
        }

        items++;
        this->Expression(node);
    }

    this->unit_->DecrementStack(items + 1); // +1 init->left

    this->unit_->Emit(vm::OpCode::INIT, (unsigned char) mode, items, &init->loc);
}

void Compiler::CompileJump(const parser::Unary *jump) {
    BasicBlock *dst = nullptr;
    String *label = nullptr;
    JBlock *jb;

    if (jump->token_type != scanner::TokenType::KW_BREAK && jump->token_type != scanner::TokenType::KW_CONTINUE) {
        this->unit_->Emit(vm::OpCode::JMP, dst, nullptr);
        return;
    }

    if (jump->value != nullptr)
        label = (String *) (((const Unary *) jump->value)->value);

    if ((jb = this->unit_->FindLoop(label)) == nullptr) {
        ErrorFormat(kCompilerError[0], "unknown loop label, the loop '%s' cannot be %s",
                    ARGON_RAW_STRING((String *) ((const Unary *) jump->value)->value),
                    jump->token_type == scanner::TokenType::KW_BREAK ? "breaked" : "continued");

        throw DatatypeException();
    }

    dst = jb->end;

    if (jump->token_type == scanner::TokenType::KW_BREAK) {
        for (auto i = 0; i < jb->pops; i++)
            this->unit_->Emit(vm::OpCode::POP, nullptr);

        // Don't decrease the stack size
        this->unit_->IncrementStack(jb->pops);
    } else if (jump->token_type == scanner::TokenType::KW_CONTINUE) {
        if (jb != this->unit_->jstack) {
            for (auto i = 0; i < jb->pops; i++)
                this->unit_->Emit(vm::OpCode::POP, nullptr);

            // Don't decrease the stack size
            this->unit_->IncrementStack(jb->pops);
        }

        dst = jb->start;
    }

    this->unit_->Emit(vm::OpCode::JMP, dst, nullptr);
}

void Compiler::CompileLoop(const parser::Loop *loop) {
    BasicBlock *begin;
    BasicBlock *end;
    JBlock *jb;

    if (!this->unit_->BlockNew())
        throw DatatypeException();

    begin = this->unit_->bb.cur;

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        jb = this->unit_->JBNew(begin, end, 0);

        if (loop->test != nullptr) {
            this->Expression(loop->test);
            this->unit_->Emit(vm::OpCode::JF, end, nullptr);
        }

        this->CompileBlock(loop->body, true);

        this->unit_->Emit(vm::OpCode::JMP, begin, nullptr);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->JBPop(jb);
    this->unit_->BlockAppend(end);
}

void Compiler::CompileLTDS(const Unary *list) {
    ARC iter;
    ARC tmp;

    vm::OpCode code;
    int items = 0;

    iter = IteratorGet(list->value, false);
    if (!iter)
        throw DatatypeException();

    while ((tmp = IteratorNext(iter.Get()))) {
        this->Expression((Node *) tmp.Get());
        items++;
    }

    switch (list->node_type) {
        case NodeType::LIST:
            code = vm::OpCode::MKLT;
            break;
        case NodeType::TUPLE:
            code = vm::OpCode::MKTP;
            break;
        case NodeType::DICT:
            code = vm::OpCode::MKDT;
            break;
        case NodeType::SET:
            code = vm::OpCode::MKST;
            break;
        default:
            throw CompilerException("invalid AST node type for CompileLTDS");
    }

    this->unit_->DecrementStack(items);

    this->unit_->Emit(code, items, nullptr, &list->loc);
}

void Compiler::CompileNullCoalescing(const parser::Binary *binary) {
    BasicBlock *end;

    this->Expression(binary->left);

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        this->unit_->Emit(vm::OpCode::JNN, end, nullptr);
        this->unit_->Emit(vm::OpCode::POP, nullptr);

        this->Expression(binary->right);
    } catch (const std::exception &) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileSafe(const parser::Unary *unary) {
    BasicBlock *end;
    JBlock *jb;

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        jb = this->unit_->JBNew((String *) nullptr, end);

        if (((Node *) unary->value)->node_type == parser::NodeType::ASSIGNMENT)
            this->Compile((const Node *) unary->value);
        else
            this->Expression((const Node *) unary->value);

        this->unit_->BlockAppend(end);

        this->unit_->JBPop(jb);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }
}

void Compiler::CompileSubscr(const parser::Subscript *subscr, bool dup, bool emit) {
    this->Expression(subscr->expression);

    if (subscr->start != nullptr)
        this->Expression(subscr->start);
    else
        this->LoadStatic((ArObject *) Nil, true, true);

    if (subscr->node_type == parser::NodeType::SLICE) {
        if (subscr->stop != nullptr)
            this->Expression(subscr->stop);
        else
            this->LoadStatic((ArObject *) Nil, true, true);

        this->unit_->Emit(vm::OpCode::MKBND, &subscr->loc);
    }

    if (dup) {
        this->unit_->Emit(vm::OpCode::DUP, 2, nullptr, nullptr);
        this->unit_->IncrementStack(2);
    }

    if (emit)
        this->unit_->Emit(vm::OpCode::SUBSCR, &subscr->loc);
}

void Compiler::CompileSwitch(const parser::Test *test) {
    BasicBlock *ltest = nullptr;
    BasicBlock *lbody = nullptr;
    BasicBlock *_default = nullptr;
    BasicBlock *bodies;
    BasicBlock *end;
    BasicBlock *tests;

    bool as_if = true;

    if (!this->unit_->BlockNew())
        throw DatatypeException();

    tests = this->unit_->bb.cur;

    if ((bodies = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    if ((end = BasicBlockNew()) == nullptr) {
        BasicBlockDel(bodies);
        throw DatatypeException();
    }

    try {
        ARC iter;
        ARC scase;

        // Compile test expression (if any)
        if (test->test != nullptr) {
            this->Expression(test->test);
            as_if = false;
        }

        iter = IteratorGet((ArObject *) test->body, false);
        if (!iter)
            throw DatatypeException();

        ltest = tests;
        lbody = bodies;

        while ((scase = IteratorNext(iter.Get()))) {
            this->CompileSwitchCase((const parser::SwitchCase *) scase.Get(), &ltest, &lbody, &_default, end,
                                    as_if);

            // Switch to test thread
            this->unit_->bb.cur = ltest;
        }

        // End of test thread
        if (!as_if)
            this->unit_->Emit(vm::OpCode::POP, nullptr);

        this->unit_->Emit(vm::OpCode::JMP, _default == nullptr ? end : _default, nullptr);
    } catch (...) {
        BasicBlockDel(bodies);
        BasicBlockDel(end);
        throw;
    }

    // Set test bodies

    // *** Remove last useless jmo instruction ***
    if (lbody != nullptr) {
        for (Instr *cursor = lbody->instr.head; cursor != nullptr; cursor = cursor->next) {
            if (cursor->next != nullptr && cursor->next->jmp == end) {
                vm::memory::Free(cursor->next);
                cursor->next = nullptr;
                lbody->instr.tail = cursor;
                lbody->size -= vm::OpCodeOffset[(int) vm::OpCode::JMP];
                break;
            }
        }
    }
    // *********************************************

    this->unit_->BlockAppend(bodies);
    this->unit_->bb.cur = lbody;

    // Set end block
    this->unit_->BlockAppend(end);
}

void Compiler::CompileSwitchCase(const SwitchCase *sw, BasicBlock **ltest, BasicBlock **lbody,
                                 BasicBlock **_default, BasicBlock *end, bool as_if) {
    ARC iter;
    ARC tmp;

    bool fallthrough = false;

    if ((*lbody)->size > 0) {
        // Switch to bodies thread
        this->unit_->bb.cur = *lbody;

        if (!this->unit_->BlockNew())
            throw DatatypeException();

        *lbody = this->unit_->bb.cur;

        // Return to test thread
        this->unit_->bb.cur = *ltest;
    }

    if (sw->conditions != nullptr) {
        iter = IteratorGet(sw->conditions, false);
        if (!iter)
            throw DatatypeException();

        while ((tmp = IteratorNext(iter.Get()))) {
            this->Expression((const Node *) tmp.Get());

            if (!as_if)
                this->unit_->Emit(vm::OpCode::TEST, &sw->loc);

            this->unit_->Emit(vm::OpCode::JT, *lbody, nullptr);

            // Save last test block
            if (!this->unit_->BlockNew())
                throw DatatypeException();

            *ltest = this->unit_->bb.cur;
        }
    }

    // Switch to bodies thread
    this->unit_->bb.cur = *lbody;

    if (sw->conditions == nullptr && *_default == nullptr)
        *_default = this->unit_->bb.cur;

    // Process body
    if (sw->body) {
        iter = IteratorGet(sw->body, false);
        if (!iter)
            throw DatatypeException();

        while ((tmp = IteratorNext(iter.Get()))) {
            const auto *node = (const Node *) tmp.Get();

            fallthrough = true;
            if (node->token_type != scanner::TokenType::KW_FALLTHROUGH) {
                fallthrough = false;
                this->Compile(node);
            }
        }
    }

    if (!fallthrough)
        this->unit_->Emit(vm::OpCode::JMP, end, nullptr);

    *lbody = this->unit_->bb.cur;
}

void Compiler::CompileTernary(const parser::Test *test) {
    BasicBlock *orelse;
    BasicBlock *end;

    if ((orelse = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    if ((end = BasicBlockNew()) == nullptr) {
        BasicBlockDel(orelse);
        throw DatatypeException();
    }

    try {
        this->Expression(test->test);

        this->unit_->Emit(vm::OpCode::JF, orelse, nullptr);

        this->Expression(test->body);

        this->unit_->Emit(vm::OpCode::JMP, end, nullptr);

        this->unit_->DecrementStack(1);

        this->unit_->BlockAppend(orelse);

        if (test->orelse != nullptr)
            this->Expression(test->orelse);
        else
            this->LoadStatic((ArObject *) Nil, true, true);

        this->unit_->BlockAppend(end);
    } catch (...) {
        BasicBlockDel(orelse);
        BasicBlockDel(end);
        throw;
    }
}

void Compiler::CompileTest(const parser::Binary *test) {
    auto *cursor = test;
    BasicBlock *end;
    int deep = 0;

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    while (cursor->left->token_type == scanner::TokenType::AND ||
           cursor->left->token_type == scanner::TokenType::OR) {
        cursor = (const parser::Binary *) cursor->left;
        deep++;
    }

    try {
        this->Expression(cursor->left);

        do {
            if (cursor->token_type == scanner::TokenType::AND)
                this->unit_->Emit(vm::OpCode::JFOP, 0, end, nullptr);
            else if (cursor->token_type == scanner::TokenType::OR)
                this->unit_->Emit(vm::OpCode::JTOP, 0, end, nullptr);
            else
                throw CompilerException("invalid TokenType for CompileTest");

            if (!this->unit_->BlockNew())
                throw DatatypeException();

            this->Expression(cursor->right);

            deep--;

            cursor = test;
            for (int i = 0; i < deep; i++)
                cursor = (const parser::Binary *) test->left;
        } while (deep >= 0);
    } catch (const std::exception &) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);
}

void Compiler::CompileTrap(const parser::Unary *unary) {
    BasicBlock *end;
    JBlock *jb;

    if ((end = BasicBlockNew()) == nullptr)
        throw DatatypeException();

    try {
        jb = this->unit_->JBNew((String *) nullptr, end);
        jb->trap = true;

        this->unit_->Emit(vm::OpCode::ST, end, &unary->loc);

        this->Expression((Node *) unary->value);
    } catch (...) {
        BasicBlockDel(end);
        throw;
    }

    this->unit_->BlockAppend(end);

    this->unit_->JBPop(jb);

    this->unit_->Emit(vm::OpCode::POPGT, this->unit_->stack.current, nullptr, nullptr);

    if (this->unit_->jstack != nullptr && this->unit_->jstack->trap)
        this->unit_->Emit(vm::OpCode::TRAP, this->unit_->jstack->end, nullptr);
    else
        this->unit_->Emit(vm::OpCode::TRAP, 0, nullptr, nullptr);
}

void Compiler::CompileUnary(const parser::Unary *unary) {
    CHECK_AST_NODE(unary, type_ast_unary_, "Compiler::Unary: invalid AST node");

    this->Expression((const Node *) unary->value);

    switch (unary->token_type) {
        case scanner::TokenType::EXCLAMATION:
            this->unit_->Emit(vm::OpCode::NOT, &unary->loc);
            break;
        case scanner::TokenType::TILDE:
            this->unit_->Emit(vm::OpCode::INV, &unary->loc);
            break;
        case scanner::TokenType::PLUS:
            this->unit_->Emit(vm::OpCode::POS, &unary->loc);
            break;
        case scanner::TokenType::MINUS:
            this->unit_->Emit(vm::OpCode::NEG, &unary->loc);
            break;
        default:
            throw CompilerException("invalid TokenType for CompileUnary");
    }
}

void Compiler::CompileUnpack(List *list, const scanner::Loc *loc) {
    ARC iter;
    ARC tmp;
    Instr *instr;

    iter = IteratorGet((ArObject *) list, false);
    if (!iter)
        throw DatatypeException();

    this->unit_->Emit(vm::OpCode::UNPACK, loc);
    instr = this->unit_->bb.cur->instr.tail;

    ArSize items = 0;
    while ((tmp = IteratorNext(iter.Get()))) {
        const auto *id = (const Unary *) tmp.Get();
        this->unit_->IncrementStack(1);
        this->StoreVariable((String *) id->value, nullptr);
        items++;
    }

    instr->oparg = items;
}

void Compiler::CompileUpdate(const parser::Unary *update) {
    const auto *value = (const Node *) update->value;

    this->Expression(value);

    this->unit_->Emit(vm::OpCode::DUP, 1, nullptr, nullptr);
    this->unit_->IncrementStack(1);

    if (update->token_type == scanner::TokenType::PLUS_PLUS)
        this->unit_->Emit(vm::OpCode::INC, &update->loc);
    else if (update->token_type == scanner::TokenType::MINUS_MINUS)
        this->unit_->Emit(vm::OpCode::DEC, &update->loc);
    else
        throw CompilerException("invalid TokenType for CompileUpdate");

    if (value->node_type == parser::NodeType::IDENTIFIER)
        this->StoreVariable((String *) ((const Unary *) value)->value, &update->loc);
    else if (value->node_type == parser::NodeType::INDEX) {
        this->CompileSubscr((const Subscript *) value, false, false);
        this->unit_->Emit(vm::OpCode::STSUBSCR, &update->loc);
    } else if (value->node_type == parser::NodeType::SELECTOR) {
        auto code = vm::OpCode::STATTR;
        if (value->token_type == scanner::TokenType::SCOPE)
            code = vm::OpCode::STSCOPE;

        auto idx = this->CompileSelector((const parser::Binary *) value, false, false);

        this->unit_->Emit(code, idx, nullptr, &update->loc);
    }
}

void Compiler::Expression(const Node *node) {
    switch (node->node_type) {
        case NodeType::AWAIT:
            this->Expression((const Node *) ((const Unary *) node)->value);
            this->unit_->Emit(vm::OpCode::AWAIT, &node->loc);
            break;
        case NodeType::CALL:
            this->CompileCall((const parser::Call *) node);
            break;
        case NodeType::FUNC:
            this->CompileFunction((const parser::Function *) node);
            break;
        case NodeType::ELVIS:
            this->CompileElvis((const parser::Binary *) node);
            break;
        case NodeType::IN:
            this->CompileContains((const parser::Binary *) node);
            break;
        case NodeType::LITERAL:
            this->LoadStatic(((const Unary *) node)->value, true, true);
            break;
        case NodeType::NULL_COALESCING:
            this->CompileNullCoalescing((const parser::Binary *) node);
            break;
        case NodeType::UNARY:
            this->CompileUnary((const Unary *) node);
            break;
        case NodeType::BINARY:
            if (node->token_type == scanner::TokenType::AND || node->token_type == scanner::TokenType::OR) {
                this->CompileTest((const parser::Binary *) node);
                break;
            }

            this->Binary((const parser::Binary *) node);
            break;
        case NodeType::TERNARY:
            this->CompileTernary((const parser::Test *) node);
            break;
        case NodeType::TRAP:
            this->CompileTrap((const Unary *) node);
            break;
        case NodeType::IDENTIFIER:
            this->LoadIdentifier((String *) ((const Unary *) node)->value);
            break;
        case NodeType::INIT:
            this->CompileInit((const parser::Initialization *) node);
            break;
        case NodeType::LIST:
        case NodeType::TUPLE:
        case NodeType::DICT:
        case NodeType::SET:
            this->CompileLTDS((const Unary *) node);
            break;
        case NodeType::UPDATE:
            this->CompileUpdate((const Unary *) node);
            break;
        case NodeType::SAFE_EXPR:
            this->CompileSafe((const Unary *) node);
            break;
        case NodeType::SELECTOR:
            this->CompileSelector((const parser::Binary *) node, false, true);
            break;
        case NodeType::INDEX:
        case NodeType::SLICE:
            this->CompileSubscr((const Subscript *) node, false, true);
            break;
        default:
            assert(false);
    }
}

void Compiler::IdentifierNew(String *name, SymbolType stype, AttributeFlag aflags, bool emit) {
    ARC sym;
    ArObject *arname;

    if (StringEqual(name, "_"))
        throw CompilerException("cannot use '_' as name of identifier");

    sym = (ArObject *) SymbolInsert(this->unit_->symt, name, stype);
    if (!sym)
        throw DatatypeException();

    auto *dest = this->unit_->names;
    auto *p_sym = (SymbolT *) sym.Get();

    p_sym->declared = true;

    if (this->unit_->symt->type == SymbolType::STRUCT || this->unit_->symt->type == SymbolType::TRAIT) {
        this->LoadStatic((ArObject *) name, true, true);
        this->unit_->Emit(vm::OpCode::NSTORE, (unsigned char) aflags, nullptr, nullptr);
        return;
    }

    if (p_sym->nested == 0) {
        auto id = p_sym->id >= 0 ? p_sym->id : dest->length;

        if (emit)
            this->unit_->Emit(vm::OpCode::NGV, (unsigned char) aflags, (unsigned short) id, nullptr);

        if (p_sym->id >= 0)
            return;
    } else {
        dest = this->unit_->locals;

        if (emit)
            this->unit_->Emit(vm::OpCode::STLC, (int) dest->length, nullptr, nullptr);
    }

    if (p_sym->id >= 0)
        arname = ListGet(!p_sym->free ? this->unit_->names : this->unit_->enclosed, p_sym->id);
    else
        arname = (ArObject *) IncRef(name);

    p_sym->id = dest->length;

    if (!ListAppend(dest, arname)) {
        Release(arname);
        throw DatatypeException();
    }

    Release(arname);
}

void Compiler::IdentifierNew(const char *name, SymbolType stype, AttributeFlag aflags, bool emit) {
    ARC id;

    id = (ArObject *) StringIntern(name);
    if (!id)
        throw DatatypeException();

    this->IdentifierNew((String *) id.Get(), stype, aflags, emit);
}

void Compiler::LoadIdentifier(String *identifier) {
    // N.B. Unknown variable, by default does not raise an error,
    // but tries to load it from the global namespace.
    SymbolT *sym;
    ArSize sym_id;

    if (StringEqual(identifier, (const char *) "_"))
        throw CompilerException("cannot use '_' as value");

    sym = this->IdentifierLookupOrCreate(identifier, SymbolType::VARIABLE);

    sym_id = sym->id;

    Release(sym);

    auto scope = this->unit_->symt->type;
    if (scope != SymbolType::STRUCT &&
        scope != SymbolType::TRAIT &&
        sym->nested > 0) {
        if (sym->declared) {
            this->unit_->Emit(vm::OpCode::LDLC, (int) sym_id, nullptr, nullptr);
            return;
        } else if (sym->free) {
            this->unit_->Emit(vm::OpCode::LDENC, (int) sym_id, nullptr, nullptr);
            return;
        }
    }

    this->unit_->Emit(vm::OpCode::LDGBL, (int) sym_id, nullptr, nullptr);
}

void Compiler::PushAtom(const char *key, bool emit) {
    ARC atom;

    atom = (ArObject *) AtomNew(key);
    if (!atom)
        throw DatatypeException();

    this->LoadStatic(atom.Get(), false, emit);
}

void Compiler::StoreVariable(String *name, const scanner::Loc *loc) {
    vm::OpCode code = vm::OpCode::STGBL;
    SymbolT *sym;

    if (StringEqual(name, (const char *) "_")) {
        this->unit_->Emit(vm::OpCode::POP, nullptr);
        return;
    }

    sym = this->IdentifierLookupOrCreate(name, SymbolType::VARIABLE);

    if (sym->declared && (this->unit_->symt->type == SymbolType::FUNC || sym->nested > 0))
        code = vm::OpCode::STLC;
    else if (sym->free)
        code = vm::OpCode::STENC;

    auto sym_id = sym->id;

    Release(sym);

    this->unit_->Emit(code, (int) sym_id, nullptr, loc);
}

void Compiler::TUScopeEnter(String *name, SymbolType context) {
    TranslationUnit *unit;
    SymbolT *symt;

    if (this->unit_ == nullptr) {
        assert(context == SymbolType::MODULE);

        if ((symt = SymbolNew(name)) == nullptr)
            throw DatatypeException();
    } else {
        if ((symt = SymbolInsert(this->unit_->symt, name, context)) == nullptr)
            throw DatatypeException();
    }

    unit = TranslationUnitNew(this->unit_, name, symt);

    Release(symt);

    if (unit == nullptr)
        throw DatatypeException();

    this->unit_ = unit;
}

void Compiler::TUScopeExit() {
    if (this->unit_ != nullptr) {
        assert(this->unit_->stack.current == 0);
        this->unit_ = TranslationUnitDel(this->unit_);
    }
}

Compiler::~Compiler() {
    while (this->unit_ != nullptr)
        this->unit_ = TranslationUnitDel(this->unit_);

    Release(this->statics_globals_);
}

Code *Compiler::Compile(File *node) {
    Code *code = nullptr;
    String *module_name;

    // Initialize global_statics
    if (this->statics_globals_ == nullptr) {
        this->statics_globals_ = DictNew();
        if (this->statics_globals_ == nullptr)
            return nullptr;
    }

    if ((module_name = StringNew(node->filename)) == nullptr)
        return nullptr;

    try {
        ARC decl_iter;
        ARC decl;

        // Let's start creating a new context
        this->TUScopeEnter(module_name, SymbolType::MODULE);

        decl_iter = IteratorGet((ArObject *) node->statements, false);
        if (!decl_iter)
            throw DatatypeException();

        // Cycle through program statements and call main compilation function
        while ((decl = IteratorNext(decl_iter.Get())))
            this->Compile((Node *) decl.Get());

        /*
         * If module is empty or last instruction is not a POP:
         *      LSTATIC nil
         *      RET
         *
         * If last instruction is POP:
         *      replace POP with RET
         *
         * These changes allow to correctly manage the output in interactive mode!
         */

        auto *last = this->unit_->bb.cur->instr.tail;
        if (last == nullptr || (vm::OpCode) last->opcode != vm::OpCode::POP) {
            this->LoadStatic((ArObject *) Nil, true, true);
            this->unit_->Emit(vm::OpCode::RET, nullptr);
        } else
            last->opcode = (unsigned char) vm::OpCode::RET;

        // EOL

        code = this->unit_->Assemble(node->doc);

        this->TUScopeExit();
    } catch (...) {
        Release(module_name);
    }

    return code;
}