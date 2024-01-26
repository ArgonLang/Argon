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
        IDENTIFIER,
        MODULE
    };

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
const argon::vm::datatype::TypeInfo *argon::lang::parser2::ExtName = &StructName##AstType

    struct Node {
        NODEOBJ_HEAD;
    };

    struct Assignment {
        NODEOBJ_HEAD;

        bool constant;
        bool multi;
        bool pub;
        bool weak;

        ArObject *name;
        ArObject *value;
    };
    _ARGONAPI extern const vm::datatype::TypeInfo *type_ast_assignment_;

    struct Module {
        NODEOBJ_HEAD;

        String *filename;
        String *docs;

        List *statements;
    };
    _ARGONAPI extern const TypeInfo *type_ast_module_;

    struct Unary {
        NODEOBJ_HEAD;

        ArObject *value;
    };
    _ARGONAPI extern const TypeInfo *type_ast_identifier_;
    _ARGONAPI extern const TypeInfo *type_ast_unary_;

    inline bool unary_dtor(Unary *self) {
        Release(self->value);
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
