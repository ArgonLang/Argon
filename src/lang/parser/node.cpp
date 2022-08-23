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