// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include <object/datatype/nil.h>
#include <object/datatype/bool.h>
#include <object/datatype/integer.h>
#include <object/datatype/decimal.h>

#include "lang/parser.h"
#include "compiler.h"

using namespace argon::memory;
using namespace argon::lang;
using namespace argon::object;

Compiler::Compiler() : Compiler(nullptr) {}

Compiler::Compiler(argon::object::Map *statics_globals) {
    if ((this->statics_globals_ = statics_globals) == nullptr) {
        if ((this->statics_globals_ = MapNew()) == nullptr)
            throw std::bad_alloc();
    }
}

Compiler::~Compiler() {
    // ???
}

argon::object::Code *Compiler::Compile(std::istream *source) {
    Parser parser(source);

    auto program = parser.Parse();

    // Let's start creating a new context
    this->EnterContext(program->filename, TUScope::MODULE);

    // Cycle through program statements and call main compilation function
    for (auto &stmt:program->body)
        this->CompileCode(stmt);

    return nullptr;
}

void Compiler::CompileCode(const ast::NodeUptr &node) {
#define TARGET_TYPE(type)   case ast::NodeType::type:

    switch (node->type) {
        TARGET_TYPE(ALIAS)
            break;
        TARGET_TYPE(ASSIGN)
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
        TARGET_TYPE(BREAK)
            break;
        TARGET_TYPE(CALL)
            break;
        TARGET_TYPE(CASE)
            break;
        TARGET_TYPE(COMMENT)
            break;
        TARGET_TYPE(CONSTANT)
            break;
        TARGET_TYPE(CONTINUE)
            break;
        TARGET_TYPE(DEFER)
            break;
        TARGET_TYPE(ELLIPSIS)
            break;
        TARGET_TYPE(ELVIS)
            break;
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
            // TODO: review
            this->CompileCode(ast::CastNode<ast::Unary>(node)->expr);
            this->EmitOp(OpCodes::POP);
            this->unit_->DecStack();
            break;
        TARGET_TYPE(FALLTHROUGH)
            break;
        TARGET_TYPE(FOR)
            break;
        TARGET_TYPE(FOR_IN)
            break;
        TARGET_TYPE(FUNC)
            break;
        TARGET_TYPE(GOTO)
            break;
        TARGET_TYPE(IDENTIFIER)
            break;
        TARGET_TYPE(IF)
            break;
        TARGET_TYPE(IMPL)
            break;
        TARGET_TYPE(IMPORT)
            break;
        TARGET_TYPE(IMPORT_FROM)
            break;
        TARGET_TYPE(IMPORT_NAME)
            break;
        TARGET_TYPE(INDEX)
            break;
        TARGET_TYPE(LABEL)
            break;
        TARGET_TYPE(LIST)
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
            break;
        TARGET_TYPE(MAP)
            break;
        TARGET_TYPE(MEMBER)
            break;
        TARGET_TYPE(NULLABLE)
            break;
        TARGET_TYPE(PROGRAM)
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
            break;
        TARGET_TYPE(SCOPE)
            break;
        TARGET_TYPE(SET)
            break;
        TARGET_TYPE(SLICE)
            break;
        TARGET_TYPE(SPAWN)
            break;
        TARGET_TYPE(STRUCT)
            break;
        TARGET_TYPE(STRUCT_INIT)
            break;
        TARGET_TYPE(SUBSCRIPT)
            break;
        TARGET_TYPE(SWITCH)
            break;
        TARGET_TYPE(TEST)
            break;
        TARGET_TYPE(TRAIT)
            break;
        TARGET_TYPE(TUPLE)
            break;
        TARGET_TYPE(UNARY_OP)
            break;
        TARGET_TYPE(UPDATE)
            break;
        TARGET_TYPE(VARIABLE)
            break;
    }

#undef TARGET_TYPE
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

void Compiler::CompileLiteral(const ast::Literal *literal) {
    ArObject *obj;

    switch (literal->kind) {
        case scanner::TokenType::STRING:
            obj = StringNew(literal->value);
            break;
        case scanner::TokenType::BYTE_STRING:
            assert(false); // TODO: BYTE_STRING
            break;
        case scanner::TokenType::RAW_STRING:
            assert(false);// TODO: RAW_STRING
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

    error:
    Release(tmp);
    Release(obj);

    return idx;
}
