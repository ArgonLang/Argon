// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_OBJECTDEF_H_
#define ARGON_VM_DATATYPE_OBJECTDEF_H_

#include <cstddef>

namespace argon::vm::datatype {
    using ArSize = size_t;
    using ArSSize = long;

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
} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_OBJECTDEF_H_
