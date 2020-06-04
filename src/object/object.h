// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_OBJECT_H_
#define ARGON_OBJECT_OBJECT_H_

#include <atomic>

#include <memory/memory.h>
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

    using VoidUnaryOp = void (*)(struct ArObject *obj);
    using BoolUnaryOp = bool (*)(struct ArObject *obj);
    using UnaryOp = struct ArObject *(*)(struct ArObject *);
    using BinaryOp = struct ArObject *(*)(struct ArObject *, struct ArObject *);
    using TernaryOp = struct ArObject *(*)(struct ArObject *, struct ArObject *, struct ArObject *);
    using CompareOp = struct ArObject *(*)(struct ArObject *, struct ArObject *, CompareMode);
    using BinaryOpArSize = struct ArObject *(*)(struct ArObject *, arsize);


    using SizeTUnaryOp = size_t (*)(struct ArObject *);
    using ArSizeUnaryOp = arsize (*)(struct ArObject *);
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
        VoidUnaryOp inc;
        VoidUnaryOp dec;
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

    struct TypeInfo {
        const unsigned char *name;
        unsigned short size;

        // Actions
        const NumberActions *number_actions;
        const SequenceActions *sequence_actions;
        const MapActions *map_actions;

        // Generic actions
        BoolUnaryOp is_true;
        BoolBinOp equal;
        CompareOp compare;
        SizeTUnaryOp hash;

        const OpSlots *ops;

        VoidUnaryOp cleanup;
    };


    struct ArObject {
        RefCount ref_count;
        const TypeInfo *type;
    };

    inline bool IsNumber(const ArObject *obj) { return obj->type->number_actions != nullptr; }

    inline bool IsSequence(const ArObject *obj) { return obj->type->sequence_actions != nullptr; }

    inline bool AsIndex(const ArObject *obj) {
        return obj->type->number_actions != nullptr && obj->type->number_actions->as_index;
    }

    inline bool IsMap(const ArObject *obj) { return obj->type->map_actions != nullptr; }

    inline bool IsTrue(const ArObject *obj) {
        if (IsSequence(obj) && obj->type->sequence_actions->length != nullptr)
            return obj->type->sequence_actions->length((ArObject *) obj) > 0;
        else if (IsMap(obj) && obj->type->map_actions->length != nullptr)
            return obj->type->map_actions->length((ArObject *) obj) > 0;
        if (obj->type->is_true != nullptr)
            return obj->type->is_true((ArObject *) obj);
        return false;
    }

    inline void IncRef(ArObject *obj) {
        if (obj != nullptr)
            obj->ref_count.IncStrong();
    };

    inline void Release(ArObject *obj) {
        if (obj == nullptr)
            return;
        if (obj->ref_count.DecStrong()) {
            if (obj->type->cleanup != nullptr)
                obj->type->cleanup(obj);
            argon::memory::Free(obj);
        }
    }

} // namespace argon::object

#endif // !ARGON_OBJECT_OBJECT_H_
