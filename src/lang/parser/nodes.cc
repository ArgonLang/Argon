// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/bool.h>

#include "nodes.h"

using namespace argon::object;
using namespace argon::lang::scanner2;
using namespace argon::lang::parser;

bool node_is_true(ArObject *self) {
    return true;
}

ArObject *unary_compare(Unary *self, ArObject *other, CompareMode mode) {
    return nullptr;
}

ArObject *binary_compare(Unary *self, ArObject *other, CompareMode mode) {
    return nullptr;
}

void unary_cleanup(Unary *self) {
    Release(self->value);
}

void binary_cleanup(Binary *self) {
    Release(self->left);
    Release(self->right);
}

void update_cleanup(UpdateIncDec *self) {
    Release(self->value);
}

void subscript_cleanup(Subscript *self) {
    Release(self->left);

    Release(self->low);
    Release(self->high);
    Release(self->step);
}

void test_cleanup(Test *self) {
    Release(self->test);

    Release(self->body);
    Release(self->orelse);
}

void assignment_cleanup(Assignment *self) {
    Release(self->name);
    Release(self->value);
}

void file_cleanup(File *self) {
    Release(self->name);
    Release(self->decls);
}

void construct_cleanup(Construct *self) {
    Release(self->name);
    Release(self->params);
    Release(self->block);
}

void loop_cleanup(Loop *self){
    Release(self->init);
    Release(self->test);
    Release(self->inc);
    Release(self->body);
}

void import_cleanup(ImportDecl *self){
    Release(self->module);
    Release(self->names);
}

#define NODE_GENERIC(name, doc, size, dtor, compare, str, obslot, ptr_name) \
const TypeInfo name##_ = {                                                  \
    TYPEINFO_STATIC_INIT,                                                   \
    #name,                                                                  \
    #doc,                                                                   \
    size,                                                                   \
    TypeInfoFlags::STRUCT,                                                  \
    nullptr,                                                                \
    (VoidUnaryOp)(dtor),                                                    \
    nullptr,                                                                \
    (CompareOp)(compare),                                                   \
    node_is_true,                                                           \
    nullptr,                                                                \
    (UnaryOp)(str),                                                         \
    nullptr,                                                                \
    nullptr,                                                                \
    nullptr,                                                                \
    nullptr,                                                                \
    nullptr,                                                                \
    nullptr,                                                                \
    nullptr,                                                                \
    nullptr,                                                                \
    nullptr,                                                                \
    nullptr,                                                                \
    nullptr                                                                 \
};                                                                          \
const TypeInfo *(ptr_name) = &(name##_)

#define UNARY_NEW(name, doc, ptr_name)  \
    NODE_GENERIC(name, doc, sizeof(argon::lang::parser::Unary), unary_cleanup, unary_compare, nullptr, nullptr, ptr_name)

#define BINARY_NEW(name, doc, ptr_name) \
    NODE_GENERIC(name, doc, sizeof(argon::lang::parser::Binary), binary_cleanup, binary_compare, nullptr, nullptr, ptr_name)

UNARY_NEW(Unary, "", argon::lang::parser::type_ast_unary_);
UNARY_NEW(Literal, "", argon::lang::parser::type_ast_literal_);
UNARY_NEW(Identifier, "", argon::lang::parser::type_ast_identifier_);
UNARY_NEW(List, "", argon::lang::parser::type_ast_list_);
UNARY_NEW(Tuple, "", argon::lang::parser::type_ast_tuple_);
UNARY_NEW(RestId, "", argon::lang::parser::type_ast_restid_);
UNARY_NEW(Spread, "", argon::lang::parser::type_ast_spread_);
UNARY_NEW(Scope, "", argon::lang::parser::type_ast_scope_);
UNARY_NEW(Map, "", argon::lang::parser::type_ast_map_);
UNARY_NEW(Set, "", argon::lang::parser::type_ast_set_);
UNARY_NEW(Expression, "", argon::lang::parser::type_ast_expression_);
UNARY_NEW(DeclList, "", argon::lang::parser::type_ast_list_decl_);
UNARY_NEW(Block, "", argon::lang::parser::type_ast_block_);
UNARY_NEW(Return, "", argon::lang::parser::type_ast_ret_);
UNARY_NEW(JmpDecl, "", argon::lang::parser::type_ast_jmp_);

BINARY_NEW(Binary, "", argon::lang::parser::type_ast_binary_);
BINARY_NEW(Selector, "", argon::lang::parser::type_ast_selector_);
BINARY_NEW(StructInit, "", argon::lang::parser::type_ast_init_);
BINARY_NEW(StructKwInit, "", argon::lang::parser::type_ast_kwinit_);
BINARY_NEW(Assignment, "", argon::lang::parser::type_ast_assignment_);
BINARY_NEW(Call, "", argon::lang::parser::type_ast_call_);
BINARY_NEW(ImportName, "", argon::lang::parser::type_ast_import_name_);
BINARY_NEW(SwitchCase, "", argon::lang::parser::type_ast_switch_case_);
BINARY_NEW(Switch, "", argon::lang::parser::type_ast_switch_);
BINARY_NEW(Label, "", argon::lang::parser::type_ast_label_);

NODE_GENERIC(LetDecl, "", sizeof(argon::lang::parser::Assignment), assignment_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_let_);

NODE_GENERIC(VarDecl, "", sizeof(argon::lang::parser::Assignment), assignment_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_var_);

NODE_GENERIC(Update, "", sizeof(argon::lang::parser::UpdateIncDec), update_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_update_);

NODE_GENERIC(Elvis, "", sizeof(argon::lang::parser::Test), test_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_elvis_);

NODE_GENERIC(Test, "", sizeof(argon::lang::parser::Test), test_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_test_);

NODE_GENERIC(Index, "", sizeof(argon::lang::parser::Subscript), subscript_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_index_);

NODE_GENERIC(Subscript, "", sizeof(argon::lang::parser::Subscript), subscript_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_subscript_);

NODE_GENERIC(File, "", sizeof(argon::lang::parser::File), file_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_file_);

NODE_GENERIC(FuncDecl, "", sizeof(argon::lang::parser::Construct), construct_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_func_);

NODE_GENERIC(TraitDecl, "", sizeof(argon::lang::parser::Construct), construct_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_trait_);

NODE_GENERIC(StructDecl, "", sizeof(argon::lang::parser::Construct), construct_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_struct_);

NODE_GENERIC(Loop, "", sizeof(argon::lang::parser::Loop), loop_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_loop_);

NODE_GENERIC(ForDecl, "", sizeof(argon::lang::parser::Loop), loop_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_for_);

NODE_GENERIC(ForInDecl, "", sizeof(argon::lang::parser::Loop), loop_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_for_in_);

NODE_GENERIC(ImportDecl, "", sizeof(argon::lang::parser::ImportDecl), import_cleanup, nullptr, nullptr, nullptr,
             argon::lang::parser::type_ast_import_decl_);

Unary *argon::lang::parser::UnaryNew(TokenType kind, Pos start, Node *right) {
    Unary *unary;

    if ((unary = ArObjectNew<Unary>(RCType::INLINE, type_ast_unary_)) != nullptr) {
        unary->kind = kind;
        unary->start = start;
        unary->end = right->end;

        unary->value = right;
    }

    return unary;
}

Unary *argon::lang::parser::SpreadNew(Node *left, scanner2::Pos end) {
    Unary *unary;

    if ((unary = ArObjectNew<Unary>(RCType::INLINE, type_ast_spread_)) != nullptr) {
        unary->start = left->start;
        unary->end = end;
        unary->colno = 0;
        unary->lineno = 0;

        unary->value = left;
    }

    return unary;
}

Binary *argon::lang::parser::BinaryNew(scanner2::TokenType kind, const TypeInfo *type, Node *left, Node *right) {
    Binary *binary;

    if ((binary = ArObjectNew<Binary>(RCType::INLINE, type)) != nullptr) {
        binary->left = left;
        binary->right = right;

        binary->start = left->start;
        binary->end = right->end;

        binary->kind = kind;
    }

    return binary;
}

Binary *argon::lang::parser::InitNew(Node *left, ArObject *args, Pos end, bool kwinit) {
    const TypeInfo *type = kwinit ? type_ast_kwinit_ : type_ast_init_;
    Binary *binary;

    if ((binary = ArObjectNew<Binary>(RCType::INLINE, type)) != nullptr) {
        binary->start = left->start;
        binary->end = end;
        binary->colno = 0;
        binary->lineno = 0;

        binary->left = left;
        binary->right = args;
    }

    return binary;
}

UpdateIncDec *argon::lang::parser::UpdateNew(TokenType kind, Pos start_end, bool prefix, Node *value) {
    UpdateIncDec *update;

    if ((update = ArObjectNew<UpdateIncDec>(RCType::INLINE, type_ast_update_)) != nullptr) {
        update->kind = kind;

        if (prefix) {
            update->start = start_end;
            update->end = value->end;
        } else {
            update->start = value->start;
            update->end = start_end;
        }

        update->colno = 0;
        update->lineno = 0;
    }

    return update;
}

Subscript *argon::lang::parser::SubscriptNew(ArObject *left, bool slice) {
    const TypeInfo *kind = slice ? type_ast_subscript_ : type_ast_index_;
    Subscript *subscript;

    if ((subscript = ArObjectNew<Subscript>(RCType::INLINE, kind)) != nullptr) {
        subscript->left = left;
        subscript->low = nullptr;
        subscript->high = nullptr;
        subscript->step = nullptr;
    }

    return subscript;
}

Assignment *argon::lang::parser::AssignmentNew(scanner2::Token &token, bool constant, bool pub, bool weak) {
    Assignment *assignment;
    const TypeInfo *type = type_ast_var_;

    if (constant)
        type = type_ast_let_;

    if ((assignment = ArObjectNew<Assignment>(RCType::INLINE, type)) != nullptr) {
        assignment->start = token.start;
        assignment->end = token.end;
        assignment->colno = 0;
        assignment->lineno = 0;

        if ((assignment->name = StringNew((const char *) token.buf)) == nullptr) {
            Release(assignment);
            return nullptr;
        }

        assignment->value = nullptr;

        assignment->pub = pub;
        assignment->weak = !constant && weak;
    }

    return assignment;
}

Construct *argon::lang::parser::FunctionNew(Pos start, String *name, List *params, Node *block, bool pub) {
    Construct *func;

    if ((func = ArObjectNew<Construct>(RCType::INLINE, type_ast_func_)) != nullptr) {
        func->start = start;
        func->end = block->end;
        func->colno = 0;
        func->lineno = 0;

        func->name = name;
        func->params = params;
        func->block = block;
        func->pub = pub;
    }

    return func;
}

ImportDecl *argon::lang::parser::ImportNew(Node *module, object::ArObject *names, scanner2::Pos start) {
    ImportDecl *imp;

    if ((imp = ArObjectNew<ImportDecl>(RCType::INLINE, type_ast_import_decl_)) != nullptr) {
        imp->start = start;
        imp->star = true;

        if (names != nullptr) {
            imp->star = false;
            if (AR_TYPEOF(names, type_list_)) {
                auto *list = (List *) names;
                imp->end = ((Node *) list->objects[list->len - 1])->end;
            } else
                imp->end = ((Node *) names)->end;
        }

        imp->colno = 0;
        imp->lineno = 0;

        imp->module = module;
        imp->names = names;
    }

    return imp;
}
