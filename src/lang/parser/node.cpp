// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "node.h"

using namespace argon::lang::parser;
using namespace argon::vm::datatype;

const argon::vm::datatype::TypeInfo BinaryType = {
        AROBJ_HEAD_INIT_TYPE,
        "Binary",
        nullptr,
        nullptr,
        sizeof(Binary),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

const argon::vm::datatype::TypeInfo FileType = {
        AROBJ_HEAD_INIT_TYPE,
        "File",
        nullptr,
        nullptr,
        sizeof(File),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

const argon::vm::datatype::TypeInfo UnaryType = {
        AROBJ_HEAD_INIT_TYPE,
        "Unary",
        nullptr,
        nullptr,
        sizeof(Unary),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

File *argon::lang::parser::FileNew(const char *filename, List *statements) {
    auto *file = NodeNew<File>(&FileType, NodeType::FILE);

    file->filename = filename;
    file->statements = IncRef(statements);

    return file;
}

Function *argon::lang::parser::FunctionNew(String *name, List *params, Node *body) {
    auto *func = NodeNew<Function>(&BinaryType, NodeType::FUNC);

    if (func != nullptr) {
        func->name = IncRef(name);
        func->params = IncRef(params);
        func->body = IncRef(body);

        func->loc.start = {};
        func->loc.end = {};

        if (body != nullptr)
            func->loc.end = body->loc.end;
    }

    return func;
}

Assignment *argon::lang::parser::AssignmentNew(ArObject *name, bool constant, bool pub, bool weak) {
    auto *assign = NodeNew<Assignment>(&BinaryType, NodeType::ASSIGNMENT);

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
        binary->loc.end = right->loc.end;
    }

    return binary;
}

Call *argon::lang::parser::CallNew(Node *left, ArObject *args, ArObject *kwargs) {
    auto *call = NodeNew<Call>(&BinaryType, NodeType::CALL);

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
    auto *cstr = NodeNew<Construct>(&BinaryType, type);

    if (cstr != nullptr) {
        cstr->name = IncRef(name);
        cstr->impls = IncRef(impls);
        cstr->body = IncRef(body);

        cstr->loc.start = {};
        cstr->loc.end = body->loc.end;
    }

    return cstr;
}

Initialization *argon::lang::parser::InitNew(Node *left, ArObject *list, const scanner::Loc &loc, bool as_map) {
    auto *init = NodeNew<Initialization>(&UnaryType, NodeType::INIT);
    if (init == nullptr)
        return nullptr;

    init->as_map = as_map;

    init->left = IncRef(left);
    init->values = IncRef(list);

    init->loc.start = left->loc.start;
    init->loc.end = loc.end;

    return init;
}

Subscript *argon::lang::parser::SubscriptNew(Node *expr, Node *start, Node *stop) {
    auto *sub = NodeNew<Subscript>(&UnaryType, stop == nullptr ? NodeType::INDEX : NodeType::SLICE);

    if (sub != nullptr) {
        sub->expression = IncRef(expr);
        sub->start = IncRef(start);
        sub->stop = IncRef(stop);

        sub->loc.start = expr->loc.start;
    }

    return sub;
}

Test *argon::lang::parser::TestNew(Node *test, Node *body, Node *orelse, NodeType type) {
    auto *tst = NodeNew<Test>(&UnaryType, type);

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