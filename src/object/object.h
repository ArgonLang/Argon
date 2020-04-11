// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_OBJECT_H_
#define ARGON_OBJECT_OBJECT_H_

#include <atomic>

#include <memory/memory.h>

namespace argon::object {
    using VoidUnaryOp = void (*)(struct ArObject *obj);
    using BoolUnaryOp = bool (*)(struct ArObject *obj);
    using UnaryOp = struct ArObject *(*)(struct ArObject *);
    using BinaryOp = struct ArObject *(*)(struct ArObject *, struct ArObject *);
    using BinaryOpSizeT = struct ArObject *(*)(struct ArObject *, size_t);

    using SizeTUnaryOp = size_t (*)(struct ArObject *);
    using BoolBinOp = bool (*)(struct ArObject *, struct ArObject *);

    struct NumberActions {
        BinaryOp idiv;
    };

    struct SequenceActions {
        SizeTUnaryOp length;
        BinaryOpSizeT get_item;
    };

    struct MapActions {
        SizeTUnaryOp length;
        BinaryOp get_item;
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
        SizeTUnaryOp hash;


        BinaryOp add;
        BinaryOp sub;
        BinaryOp mul;
        BinaryOp div;
        BinaryOp module;
        BinaryOp lsh;
        BinaryOp rsh;

        VoidUnaryOp cleanup;
    };


    struct ArObject {
        std::atomic_uintptr_t strong_or_ref;
        const TypeInfo *type;
    };

    inline bool IsNumber(const ArObject *obj) { return obj->type->number_actions != nullptr; }

    inline bool IsSequence(const ArObject *obj) { return obj->type->sequence_actions != nullptr; }

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

    inline void IncRef(ArObject *obj) { obj->strong_or_ref++; };

    inline void Release(ArObject *obj) {
        if (obj->strong_or_ref.fetch_sub(1) == 1) {
            if (obj->type->cleanup != nullptr)
                obj->type->cleanup(obj);
            argon::memory::Free(obj);
        }
    }

} // namespace argon::object

#endif // !ARGON_OBJECT_OBJECT_H_
