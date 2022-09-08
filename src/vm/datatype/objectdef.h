// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_OBJECTDEF_H_
#define ARGON_VM_DATATYPE_OBJECTDEF_H_

#include <cstddef>

#include <util/enum_bitmask.h>

namespace argon::vm::datatype {
    using ArSize = size_t;
    using ArSSize = long;

    enum class BufferFlags {
        READ,
        WRITE
    };

    enum class CompareMode {
        EQ = 0,
        NE = 1,
        GR = 2,
        GRQ = 3,
        LE = 4,
        LEQ = 5
    };

    using ArSize_UnaryOp = ArSize (*)(const struct ArObject *);
    using BinaryOp = struct ArObject *(*)(struct ArObject *, struct ArObject *);
    using Bool_TernaryOp = bool (*)(struct ArObject *, struct ArObject *, struct ArObject *);
    using Bool_UnaryOp = bool (*)(const struct ArObject *);
    using CompareOp = struct ArObject *(*)(const struct ArObject *, const struct ArObject *, CompareMode);
    using UnaryOp = struct ArObject *(*)(struct ArObject *);
    using UnaryConstOp = struct ArObject *(*)(const struct ArObject *);
    using UnaryBoolOp = struct ArObject *(*)(struct ArObject *, bool);
    using VariadicOp = struct ArObject *(*)(const struct TypeInfo *, ArObject **, ArSize);
    using Void_UnaryOp = void (*)(struct ArObject *);

    using TraceOp = void (*)(struct ArObject *, Void_UnaryOp);

    struct ArBuffer {
        ArObject *object;

        unsigned char *buffer;

        struct {
            ArSize item_size;
            ArSize nelem;
        } geometry;

        ArSize length;
        BufferFlags flags;
    };

    using BufferGetFn = bool (*)(struct ArObject *object, ArBuffer *buffer, BufferFlags flags);
    using BufferRelFn = void (*)(ArBuffer *buffer);

} // namespace argon::vm::datatype

ENUMBITMASK_ENABLE(argon::vm::datatype::BufferFlags);

#endif // !ARGON_VM_DATATYPE_OBJECTDEF_H_
