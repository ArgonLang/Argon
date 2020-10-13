// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_AROBJECT_H_
#define ARGON_OBJECT_AROBJECT_H_

#include <atomic>
#include <cstddef>

#include "refcount.h"

namespace argon::object {
    enum class CompareMode : unsigned char {
        EQ,
        NE,
        GE,
        GEQ,
        LE,
        LEQ
    };

    using arsize = long;

    using UnaryOp = struct ArObject *(*)(struct ArObject *);
    using BinaryOp = struct ArObject *(*)(struct ArObject *, struct ArObject *);
    using TernaryOp = struct ArObject *(*)(struct ArObject *, struct ArObject *, struct ArObject *);
    using CompareOp = struct ArObject *(*)(struct ArObject *, struct ArObject *, CompareMode);
    using BinaryOpArSize = struct ArObject *(*)(struct ArObject *, arsize);

    using VoidUnaryOp = void (*)(struct ArObject *obj);
    using Trace = void (*)(struct ArObject *, VoidUnaryOp);
    using SizeTUnaryOp = size_t (*)(struct ArObject *);
    using ArSizeUnaryOp = arsize (*)(struct ArObject *);
    using BoolUnaryOp = bool (*)(struct ArObject *obj);
    using BoolBinOp = bool (*)(struct ArObject *, struct ArObject *);
    using BoolTernOp = bool (*)(struct ArObject *, struct ArObject *, struct ArObject *);
    using BoolTernOpArSize = bool (*)(struct ArObject *, struct ArObject *, arsize);

    struct OpSlots {
        // Math
        BinaryOp add;
        BinaryOp sub;
        BinaryOp mul;
        BinaryOp div;
        BinaryOp idiv;
        BinaryOp module;
        UnaryOp pos;
        UnaryOp neg;

        // Logical op
        BinaryOp l_and;
        BinaryOp l_or;
        BinaryOp l_xor;
        BinaryOp shl;
        BinaryOp shr;
        UnaryOp invert;

        // Inplace update
        BinaryOp inp_add;
        BinaryOp inp_sub;
        BinaryOp inp_mul;
        BinaryOp inp_div;
        UnaryOp inc;
        UnaryOp dec;
    };

    struct NumberActions {
        UnaryOp as_number;
        ArSizeUnaryOp as_index;
    };

    struct SequenceActions {
        SizeTUnaryOp length;
        BinaryOpArSize get_item;
        BoolTernOpArSize set_item;
        BinaryOp get_slice;
        BoolTernOp set_slice;
    };

    struct MapActions {
        SizeTUnaryOp length;
        BinaryOp get_item;
        BoolTernOp set_item;
    };

    struct ObjectActions {
        BinaryOp get_attr;
        BinaryOp get_static_attr;
        BoolTernOp set_attr;
        BoolTernOp set_static_attr;
    };

    struct TypeInfo {
        const unsigned char *name;
        unsigned short size;

        // Actions
        const NumberActions *number_actions;
        const SequenceActions *sequence_actions;
        const MapActions *map_actions;
        const ObjectActions *obj_actions;

        // Generic actions
        BoolUnaryOp is_true;
        BoolBinOp equal;
        CompareOp compare;
        SizeTUnaryOp hash;
        UnaryOp str;

        const OpSlots *ops;

        Trace trace;
        VoidUnaryOp cleanup;
    };


    struct ArObject {
        RefCount ref_count;
        const TypeInfo *type;
    };

} // namespace argon::object

#endif // !ARGON_OBJECT_AROBJECT_H_
