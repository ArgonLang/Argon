// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER2_NODE_NODE_H_
#define ARGON_LANG_PARSER2_NODE_NODE_H_

#include <argon/lang/scanner/token.h>

#include <argon/vm/memory/memory.h>

#include <argon/vm/datatype/objectdef.h>
#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/list.h>

namespace argon::lang::parser2::node {
    using namespace argon::vm::datatype;

#define NODEOBJ_HEAD                            \
    AROBJ_HEAD;                                 \
    const NodeType node_type;                   \
    argon::lang::scanner::TokenType token_type; \
    argon::lang::scanner::Loc loc

    enum class NodeType {
        ARGUMENT,
        ASSERTION,
        ASSIGNMENT,
        AWAIT,
        BLOCK,
        CALL,
        DICT,
        ELVIS,
        EXPRESSION,
        FOR,
        FOREACH,
        FUNCTION,
        IDENTIFIER,
        IF,
        IMPORT,
        IMPORT_NAME,
        INDEX,
        IN,
        JUMP,
        INFIX,
        LABEL,
        LIST,
        LITERAL,
        LOOP,
        MODULE,
        NOT_IN,
        NULL_COALESCING,
        OBJ_INIT,
        KWARG,
        KWPARAM,
        PANIC,
        PARAMETER,
        PREFIX,
        REST,
        RETURN,
        SAFE_EXPR,
        SELECTOR,
        SET,
        SLICE,
        SPREAD,
        STRUCT,
        SWITCH,
        SWITCH_CASE,
        SYNC_BLOCK,
        TERNARY,
        TRAIT,
        TRAP,
        TUPLE,
        UPDATE,
        VARDECL,
        YIELD
    };

#define NODE_NEW(StructName, ExtName, alias, doc, dtor, compare)    \
TypeInfo alias##AstType = {               \
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
const argon::vm::datatype::TypeInfo *argon::lang::parser2::ExtName = &alias##AstType

    struct Node {
        NODEOBJ_HEAD;
    };

    struct Assignment {
        NODEOBJ_HEAD;

        ArObject *name;
        ArObject *value;

        bool constant;
        bool multi;
        bool pub;
        bool weak;
    };
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_assignment_;
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_vardecl_;

    struct Binary {
        NODEOBJ_HEAD;

        ArObject *left;
        ArObject *right;
    };
    _ARGONAPI extern const TypeInfo *type_ast_assertion_;
    _ARGONAPI extern const TypeInfo *type_ast_binary_;
    _ARGONAPI extern const TypeInfo *type_ast_import_name_;
    _ARGONAPI extern const TypeInfo *type_ast_infix_;
    _ARGONAPI extern const TypeInfo *type_ast_selector_;
    _ARGONAPI extern const TypeInfo *type_ast_switchcase_;
    _ARGONAPI extern const TypeInfo *type_ast_sync_;

    struct Branch {
        NODEOBJ_HEAD;

        Node *test;
        Node *body;
        Node *orelse;
    };
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_branch_;

    struct Call {
        NODEOBJ_HEAD;

        Node *left;
        List *args;
        List *kwargs;
    };
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_call_;

    struct Construct {
        NODEOBJ_HEAD;

        String *name;
        String *doc;
        List *impls;

        Node *body;

        bool pub;
    };
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_struct_;
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_trait_;

    struct Function {
        NODEOBJ_HEAD;

        String *name;
        String *doc;

        List *params;
        Node *body;

        bool async;
        bool pub;
    };
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_function_;

    struct Import {
        NODEOBJ_HEAD;

        Node *mod;
        ArObject *names;

        bool pub;
    };
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_import_;

    struct Loop {
        NODEOBJ_HEAD;

        Node *init;
        Node *test;
        Node *inc;
        Node *body;
    };
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_loop_;

    struct Module {
        NODEOBJ_HEAD;

        String *filename;
        String *docs;

        List *statements;
    };
    _ARGONAPI extern const TypeInfo *type_ast_module_;

    struct ObjectInit {
        NODEOBJ_HEAD;

        Node *left;
        ArObject *values;

        bool as_map;
    };
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_objinit_;

    struct Parameter {
        NODEOBJ_HEAD;

        String *id;
        Node *value;
    };
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_argument_;
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_parameter_;

    struct Subscript {
        NODEOBJ_HEAD;

        Node *expression;
        Node *start;
        Node *stop;
    };
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_subscript_;

    struct Unary {
        NODEOBJ_HEAD;

        ArObject *value;
    };
    _ARGONAPI extern const TypeInfo *type_ast_identifier_;
    _ARGONAPI extern const TypeInfo *type_ast_jump_;
    _ARGONAPI extern const TypeInfo *type_ast_literal_;
    _ARGONAPI extern const TypeInfo *type_ast_prefix_;
    _ARGONAPI extern const TypeInfo *type_ast_unary_;
    _ARGONAPI extern const TypeInfo *type_ast_update_;

    inline bool unary_dtor(Unary *self) {
        Release(self->value);

        return true;
    }

    inline bool binary_dtor(Binary *self) {
        Release(self->left);
        Release(self->right);

        return true;
    }


    template<typename T>
    T *NewNode(const TypeInfo *t_info, bool gc, NodeType node_type) {
        T *node;

        node = !gc ? MakeObject<T>(t_info) : MakeGCObject<T>(t_info);

        if (node == nullptr)
            return nullptr;

        auto *n_obj = (((unsigned char *) node) + sizeof(ArObject));

        vm::memory::MemoryZero(n_obj, t_info->size - sizeof(ArObject));

        *((NodeType *) n_obj) = node_type;

        if (gc)
            argon::vm::memory::Track((ArObject *) node);

        return node;
    }

    Unary *SafeExprNew(Node *node);

} // namespace argon::lang::parser2

#endif // ARGON_LANG_PARSER2_NODE_NODE_H_
