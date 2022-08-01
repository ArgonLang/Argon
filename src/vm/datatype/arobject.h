// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_AROBJECT_H_
#define ARGON_VM_DATATYPE_AROBJECT_H_

#include <cstddef>

#include <vm/memory/refcount.h>

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

    using ArSize_UnaryOp = ArSize (*)(struct AroObject *);
    using BinaryOp = struct ArObject *(*)(struct ArObject *, struct ArObject *);
    using Bool_UnaryOp = bool (*)(struct ArObject *);
    using CompareOp = struct ArObject *(*)(struct ArObject *, struct ArObject *, CompareMode);
    using UnaryOp = struct ArObject *(*)(struct ArObject *);
    using VariadicOp = struct ArObject *(*)(const struct TypeInfo *, ArObject **, ArSize);
    using Void_UnaryOp = void (*)(struct ArObject *);

    using TraceOp = void (*)(struct ArObject *, Void_UnaryOp);

#define AROBJ_HEAD                  \
    memory::RefCount ref_count_;    \
    const struct TypeInfo *type_

    enum class TypeInfoFlags {

    };

    struct TypeInfo {
        AROBJ_HEAD;

        const char *name;
        const char *doc;

        VariadicOp ctor;
        VariadicOp dtor;
        TraceOp trace;

        /* Return datatype hash */
        ArSize_UnaryOp hash;

        /* Return datatype truthiness */
        Bool_UnaryOp is_true;

        /* Make this datatype comparable */
        CompareOp compare;

        /* Return string datatype representation */
        UnaryOp repr;

        /* Return string datatype representation */
        UnaryOp str;
    };

#define AR_GET_TYPE(object)         ((object)->type_)
#define AR_SAME_TYPE(object, other) (AR_GET_TYPE(object) == AR_GET_TYPE(other))
#define AR_TYPE_NAME(object)        (AR_GET_TYPE(object)->name)
#define AR_TYPEOF(object, type)     (AR_GET_TYPE(object) == (type))

    struct ArObject {
        AROBJ_HEAD;
    };

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_AROBJECT_H_
