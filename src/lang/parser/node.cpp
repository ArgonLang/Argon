// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/memory/memory.h>

#include "node.h"

using namespace argon::lang::parser;
using namespace argon::vm::datatype;

#define NODE_NEW(StructName, alias, doc, dtor, compare)     \
const argon::vm::datatype::TypeInfo StructName##Type = {    \
        AROBJ_HEAD_INIT_TYPE,                               \
        #alias,                                             \
        nullptr,                                            \
        nullptr,                                            \
        sizeof(StructName),                                 \
        TypeInfoFlags::BASE,                                \
        nullptr,                                            \
        (Bool_UnaryOp) (dtor),                              \
        nullptr,                                            \
        nullptr,                                            \
        nullptr,                                            \
        (compare),                                          \
        nullptr,                                            \
        nullptr,                                            \
        nullptr,                                            \
        nullptr,                                            \
        nullptr,                                            \
        nullptr,                                            \
        nullptr,                                            \
        nullptr,                                            \
        nullptr,                                            \
        nullptr,                                            \
        nullptr }

// DTORs

bool assignment_dtor(Assignment *self) {
    Release(self->name);
    Release(self->value);

    return true;
}

bool binary_dtor(Binary *self) {
    Release(self->left);
    Release(self->right);

    return true;
}

bool call_dtor(Call *self) {
    Release(self->left);
    Release(self->args);
    Release(self->kwargs);

    return true;
}

bool construct_dtor(Construct *self) {
    Release(self->name);
    Release(self->doc);
    Release(self->impls);
    Release(self->body);

    return true;
}

bool file_dtor(File *self) {
    argon::vm::memory::Free((void *) self->filename);
    Release(self->doc);
    Release(self->statements);

    return true;
}

bool function_dtor(Function *self) {
    Release(self->name);
    Release(self->doc);
    Release(self->params);
    Release(self->body);

    return true;
}

bool import_dtor(Import *self) {
    Release(self->mod);
    Release(self->names);

    return true;
}

bool initialization_dtor(Initialization *self) {
    Release(self->left);
    Release(self->values);

    return true;
}

bool loop_dtor(Loop *self) {
    Release(self->init);
    Release(self->test);
    Release(self->inc);
    Release(self->body);

    return true;
}

bool subscript_dtor(Subscript *self) {
    Release(self->expression);
    Release(self->start);
    Release(self->stop);

    return true;
}

bool switchcase_dtor(SwitchCase *self) {
    Release(self->conditions);
    Release(self->body);

    return true;
}

bool test_dtor(Test *self) {
    Release(self->test);
    Release(self->body);
    Release(self->orelse);

    return true;
}

bool unary_dtor(Unary *self) {
    Release(self->value);

    return true;
}

// TYPEs DEF

NODE_NEW(Assignment, Assignment, nullptr, assignment_dtor, nullptr);
NODE_NEW(Binary, Binary, nullptr, binary_dtor, nullptr);
NODE_NEW(Call, Call, nullptr, call_dtor, nullptr);
NODE_NEW(Construct, Construct, nullptr, construct_dtor, nullptr);
NODE_NEW(File, File, nullptr, file_dtor, nullptr);
NODE_NEW(Function, Function, nullptr, function_dtor, nullptr);
NODE_NEW(Import, Import, nullptr, import_dtor, nullptr);
NODE_NEW(Initialization, Initialization, nullptr, initialization_dtor, nullptr);
NODE_NEW(Loop, Loop, nullptr, loop_dtor, nullptr);
NODE_NEW(Subscript, Subscript, nullptr, subscript_dtor, nullptr);
NODE_NEW(SwitchCase, SwitchCase, nullptr, switchcase_dtor, nullptr);
NODE_NEW(Test, Test, nullptr, test_dtor, nullptr);
NODE_NEW(Unary, Unary, nullptr, unary_dtor, nullptr);

File *argon::lang::parser::FileNew(const char *filename, List *statements) {
    auto *file = NodeNew<File>(&FileType, NodeType::FILE);

    file->filename = filename;
    file->statements = IncRef(statements);

    return file;
}

Function *argon::lang::parser::FunctionNew(String *name, List *params, Node *body, bool pub) {
    auto *func = NodeNew<Function>(&FunctionType, NodeType::FUNC);

    if (func != nullptr) {

        func->async = false;
        func->pub = pub;

        func->name = IncRef(name);
        func->doc = nullptr;
        func->params = IncRef(params);
        func->body = IncRef(body);

        func->loc.start = {};
        func->loc.end = {};

        if (body != nullptr)
            func->loc.end = body->loc.end;
    }

    return func;
}

Import *argon::lang::parser::ImportNew(Node *mod, ArObject *names) {
    auto *imp = NodeNew<Import>(&ImportType, NodeType::IMPORT);

    if (imp != nullptr) {
        imp->mod = IncRef(mod);
        imp->names = IncRef(names);

        imp->loc = {};
    }

    return imp;
}

Assignment *argon::lang::parser::AssignmentNew(ArObject *name, bool constant, bool pub, bool weak) {
    auto *assign = NodeNew<Assignment>(&AssignmentType, NodeType::DECLARATION);

    if (assign != nullptr) {
        assign->constant = constant;

        assign->multi = AR_TYPEOF(name, type_list_);

        assign->pub = pub;
        assign->weak = weak;

        assign->name = IncRef(name);
        assign->value = nullptr;
    }

    return assign;
}

Binary *argon::lang::parser::BinaryNew(Node *left, Node *right, scanner::TokenType token, NodeType type) {
    auto *binary = NodeNew<Binary>(&BinaryType, type);

    if (binary != nullptr) {
        binary->left = IncRef(left);
        binary->right = IncRef(right);

        binary->token_type = token;

        binary->loc.start = left->loc.start;

        binary->loc.end = {};

        if (right != nullptr)
            binary->loc.end = right->loc.end;
    }

    return binary;
}

Call *argon::lang::parser::CallNew(Node *left, ArObject *args, ArObject *kwargs) {
    auto *call = NodeNew<Call>(&CallType, NodeType::CALL);

    if (call != nullptr) {
        call->left = IncRef(left);
        call->args = IncRef(args);
        call->kwargs = IncRef(kwargs);


        call->loc.start = left->loc.start;
        call->loc.end = {};
    }

    return call;
}

Construct *argon::lang::parser::ConstructNew(String *name, List *impls, Node *body, NodeType type) {
    auto *cstr = NodeNew<Construct>(&ConstructType, type);

    if (cstr != nullptr) {
        cstr->name = IncRef(name);
        cstr->doc = nullptr;
        cstr->impls = IncRef(impls);
        cstr->body = IncRef(body);

        cstr->loc.start = {};
        cstr->loc.end = body->loc.end;
    }

    return cstr;
}

Initialization *argon::lang::parser::InitNew(Node *left, ArObject *list, const scanner::Loc &loc, bool as_map) {
    auto *init = NodeNew<Initialization>(&InitializationType, NodeType::INIT);
    if (init == nullptr)
        return nullptr;

    init->as_map = as_map;

    init->left = IncRef(left);
    init->values = IncRef(list);

    init->loc.start = left->loc.start;
    init->loc.end = loc.end;

    return init;
}

Loop *argon::lang::parser::LoopNew(Node *init, Node *test, Node *inc, Node *body, NodeType type) {
    auto *loop = NodeNew<Loop>(&LoopType, type);

    if (loop != nullptr) {
        loop->init = IncRef(init);
        loop->test = IncRef(test);
        loop->inc = IncRef(inc);
        loop->body = IncRef(body);

        loop->loc = {};

        loop->loc.end = loop->body->loc.end;
    }

    return loop;
}

Subscript *argon::lang::parser::SubscriptNew(Node *expr, Node *start, Node *stop) {
    auto *sub = NodeNew<Subscript>(&SubscriptType, stop == nullptr ? NodeType::INDEX : NodeType::SLICE);

    if (sub != nullptr) {
        sub->expression = IncRef(expr);
        sub->start = IncRef(start);
        sub->stop = IncRef(stop);

        sub->loc.start = expr->loc.start;
    }

    return sub;
}

SwitchCase *argon::lang::parser::SwitchCaseNew(ArObject *conditions, ArObject *body, const scanner::Loc &loc) {
    auto *sw = NodeNew<SwitchCase>(&SwitchCaseType, NodeType::SWITCH_CASE);

    if (sw != nullptr) {
        sw->conditions = IncRef(conditions);
        sw->body = IncRef(body);

        sw->loc = loc;
    }

    return sw;
}

Test *argon::lang::parser::TestNew(Node *test, Node *body, Node *orelse, NodeType type) {
    auto *tst = NodeNew<Test>(&TestType, type);

    if (tst != nullptr) {
        tst->test = IncRef(test);
        tst->body = IncRef(body);
        tst->orelse = IncRef(orelse);

        tst->loc.start = test->loc.start;

        if (tst->orelse != nullptr)
            tst->loc.end = orelse->loc.end;
    }

    return tst;
}

Unary *argon::lang::parser::UnaryNew(ArObject *value, NodeType type, const scanner::Loc &loc) {
    auto *unary = NodeNew<Unary>(&UnaryType, type);

    if (unary != nullptr) {
        unary->loc = loc;
        unary->value = IncRef(value);
    }

    return unary;
}