// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/memory/memory.h>

#include <argon/lang/parser/node.h>

using namespace argon::lang::parser;
using namespace argon::vm::datatype;

#define NODE_NEW(StructName, ExtName, alias, doc, dtor, compare)    \
TypeInfo StructName##AstType = {               \
        AROBJ_HEAD_INIT_TYPE,                                       \
        #alias,                                                     \
        nullptr,                                                    \
        nullptr,                                                    \
        sizeof(StructName),                                         \
        TypeInfoFlags::BASE,                                        \
        nullptr,                                                    \
        (Bool_UnaryOp) (dtor),                                      \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        (compare),                                                  \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr,                                                    \
        nullptr };                                                  \
const argon::vm::datatype::TypeInfo *argon::lang::parser::ExtName = &StructName##AstType

// DTORs

bool argument_dtor(Argument *self) {
    Release(self->id);
    Release(self->value);

    return true;
}

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

NODE_NEW(Argument, type_ast_argument_, Argument, nullptr, argument_dtor, nullptr);
NODE_NEW(Assignment, type_ast_assignment_, Assignment, nullptr, assignment_dtor, nullptr);
NODE_NEW(Binary, type_ast_binary_, Binary, nullptr, binary_dtor, nullptr);
NODE_NEW(Call, type_ast_call_, Call, nullptr, call_dtor, nullptr);
NODE_NEW(Construct, type_ast_construct_, Construct, nullptr, construct_dtor, nullptr);
NODE_NEW(File, type_ast_file_, File, nullptr, file_dtor, nullptr);
NODE_NEW(Function, type_ast_function_, Function, nullptr, function_dtor, nullptr);
NODE_NEW(Import, type_ast_import_, Import, nullptr, import_dtor, nullptr);
NODE_NEW(Initialization, type_ast_initialization_, Initialization, nullptr, initialization_dtor, nullptr);
NODE_NEW(Loop, type_ast_loop_, Loop, nullptr, loop_dtor, nullptr);
NODE_NEW(Subscript, type_ast_subscript_, Subscript, nullptr, subscript_dtor, nullptr);
NODE_NEW(SwitchCase, type_ast_switchcase_, SwitchCase, nullptr, switchcase_dtor, nullptr);
NODE_NEW(Test, type_ast_test_, Test, nullptr, test_dtor, nullptr);
NODE_NEW(Unary, type_ast_unary_, Unary, nullptr, unary_dtor, nullptr);

Argument *argon::lang::parser::ArgumentNew(Unary *id, Node *def_value, NodeType type) {
    auto *param = NodeNew<Argument>(&ArgumentAstType, type);

    if (param != nullptr) {
        param->id = (Node *) IncRef(id);
        param->value = IncRef(def_value);

        if (id != nullptr)
            param->loc = id->loc;

        if (def_value != nullptr)
            param->loc.end = def_value->loc.end;
    }

    return param;
}

Assignment *argon::lang::parser::AssignmentNew(ArObject *name, bool constant, bool pub, bool weak) {
    auto *assign = NodeNew<Assignment>(&AssignmentAstType, NodeType::DECLARATION);

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
    auto *binary = NodeNew<Binary>(&BinaryAstType, type);

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
    auto *call = NodeNew<Call>(&CallAstType, NodeType::CALL);

    if (call != nullptr) {
        call->token_type = scanner::TokenType::TK_NULL;

        call->left = IncRef(left);
        call->args = IncRef(args);
        call->kwargs = IncRef(kwargs);

        call->loc.start = left->loc.start;
        call->loc.end = {};
    }

    return call;
}

Construct *argon::lang::parser::ConstructNew(String *name, List *impls, Node *body, NodeType type, bool pub) {
    auto *cstr = NodeNew<Construct>(&ConstructAstType, type);

    if (cstr != nullptr) {
        cstr->pub = pub;
        cstr->name = IncRef(name);
        cstr->doc = nullptr;
        cstr->impls = IncRef(impls);
        cstr->body = IncRef(body);

        cstr->loc.start = {};
        cstr->loc.end = body->loc.end;
    }

    return cstr;
}

File *argon::lang::parser::FileNew(const char *filename, List *statements) {
    auto *file = NodeNew<File>(&FileAstType, NodeType::FILE);
    size_t length = strlen(filename);

    if (file != nullptr) {
        if ((file->filename = (char *) vm::memory::Alloc(length + 1)) == nullptr) {
            Release(file);
            return nullptr;
        }

        vm::memory::MemoryCopy(file->filename, filename, length);

        file->filename[length] = '\0';

        file->statements = IncRef(statements);
    }

    return file;
}

Function *argon::lang::parser::FunctionNew(String *name, List *params, Node *body, bool pub) {
    auto *func = NodeNew<Function>(&FunctionAstType, NodeType::FUNC);

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

Import *argon::lang::parser::ImportNew(Node *mod, ArObject *names, bool pub) {
    auto *imp = NodeNew<Import>(&ImportAstType, NodeType::IMPORT);

    if (imp != nullptr) {
        imp->pub = pub;

        imp->mod = IncRef(mod);
        imp->names = IncRef(names);

        imp->loc = {};
    }

    return imp;
}

Initialization *argon::lang::parser::InitNew(Node *left, ArObject *list, const scanner::Loc &loc, bool as_map) {
    auto *init = NodeNew<Initialization>(&InitializationAstType, NodeType::INIT);
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
    auto *loop = NodeNew<Loop>(&LoopAstType, type);

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

Subscript *argon::lang::parser::SubscriptNew(Node *expr, Node *start, Node *stop, bool slice) {
    auto *sub = NodeNew<Subscript>(&SubscriptAstType, slice ? NodeType::SLICE : NodeType::INDEX);

    if (sub != nullptr) {
        sub->expression = IncRef(expr);
        sub->start = IncRef(start);
        sub->stop = IncRef(stop);

        sub->loc.start = expr->loc.start;
    }

    return sub;
}

SwitchCase *argon::lang::parser::SwitchCaseNew(ArObject *conditions, ArObject *body, const scanner::Loc &loc) {
    auto *sw = NodeNew<SwitchCase>(&SwitchCaseAstType, NodeType::SWITCH_CASE);

    if (sw != nullptr) {
        sw->conditions = IncRef(conditions);
        sw->body = IncRef(body);

        sw->loc = loc;
    }

    return sw;
}

Test *argon::lang::parser::TestNew(Node *test, Node *body, Node *orelse, NodeType type) {
    auto *tst = NodeNew<Test>(&TestAstType, type);

    if (tst != nullptr) {
        tst->test = IncRef(test);
        tst->body = IncRef(body);
        tst->orelse = IncRef(orelse);

        if (test != nullptr)
            tst->loc.start = test->loc.start;

        if (tst->orelse != nullptr)
            tst->loc.end = orelse->loc.end;
    }

    return tst;
}

Unary *argon::lang::parser::UnaryNew(ArObject *value, NodeType type, const scanner::Loc &loc) {
    auto *unary = NodeNew<Unary>(&UnaryAstType, type);

    if (unary != nullptr) {
        unary->loc = loc;
        unary->value = IncRef(value);
    }

    return unary;
}
