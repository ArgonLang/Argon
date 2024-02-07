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
        ASSIGNMENT,
        AWAIT,
        BLOCK,
        CHAN_IN,
        CHAN_OUT,
        DICT,
        FUNCTION,
        IDENTIFIER,
        IMPORT,
        IMPORT_NAME,
        INDEX,
        IN,
        INFIX,
        LIST,
        LITERAL,
        MODULE,
        NOT_IN,
        KWPARAM,
        PARAMETER,
        PREFIX,
        REST,
        SELECTOR,
        SET,
        SLICE,
        STRUCT,
        SYNC_BLOCK,
        TRAIT,
        TRAP,
        TUPLE,
        UNARY,
        UPDATE
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

    struct Binary {
        NODEOBJ_HEAD;

        ArObject *left;
        ArObject *right;
    };
    _ARGONAPI extern const TypeInfo *type_ast_binary_;
    _ARGONAPI extern const TypeInfo *type_ast_import_name_;
    _ARGONAPI extern const TypeInfo *type_ast_infix_;
    _ARGONAPI extern const TypeInfo *type_ast_selector_;
    _ARGONAPI extern const TypeInfo *type_ast_sync_;

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

    struct Module {
        NODEOBJ_HEAD;

        String *filename;
        String *docs;

        List *statements;
    };
    _ARGONAPI extern const TypeInfo *type_ast_module_;

    struct Parameter {
        NODEOBJ_HEAD;

        String *id;
        Node *value;
    };
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

        if (!gc)
            node = MakeObject<T>(t_info);
        else
            node = MakeGCObject<T>(t_info, false);

        if (node == nullptr)
            return nullptr;

        auto *n_obj = (((unsigned char *) node) + sizeof(ArObject));

        vm::memory::MemoryZero(n_obj, t_info->size - sizeof(ArObject));

        *((NodeType *) n_obj) = node_type;

        return node;
    }
} // namespace argon::lang::parser2

#endif // ARGON_LANG_PARSER2_NODE_NODE_H_
