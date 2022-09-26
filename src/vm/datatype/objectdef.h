// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_OBJECTDEF_H_
#define ARGON_VM_DATATYPE_OBJECTDEF_H_

#include <cassert>
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

#define ARGON_RICH_COMPARE_CASES(a, b, mode)    \
    switch (mode) {                             \
        case CompareMode::EQ:                   \
            return BoolToArBool((a) == (b));    \
        case CompareMode::NE:                   \
            assert(false);                      \
            break;                              \
        case CompareMode::GR:                   \
            return BoolToArBool((a) > (b));     \
        case CompareMode::GRQ:                  \
            return BoolToArBool((a) >= (b));    \
        case CompareMode::LE:                   \
            return BoolToArBool((a) < (b));     \
        case CompareMode::LEQ:                  \
            return BoolToArBool((a) <= (b));    \
        default:                                \
            assert(false);                      \
    }                                           \

    enum class TypeInfoFlags : unsigned int {
        // BIT FLAGS | OBJECT TYPE
        //                       ^--- Two bits to represent the type of object.
        BASE = 0,
        TRAIT = 1,
        STRUCT = 2,

        // BIT_FLAGS
        INITIALIZED = 1 << 2,
        WEAKABLE = 1 << 3
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

    using FunctionPtr = ArObject *(*)(ArObject *, ArObject *, ArObject **, ArObject *, ArSize);

    struct FunctionDef {
        /* Name of native function (this name will be exposed to Argon) */
        const char *name;

        /* Documentation of native function (this doc will be exposed to Argon) */
        const char *doc;

        /* Pointer to native code */
        FunctionPtr func;

        /* Arity of the function, how many args accepts in input?! */
        unsigned short arity;

        /* Is a variadic function? (func variadic(p1,p2,...p3)) */
        bool variadic;

        /* Can it accept keyword parameters?? (func kwargs(p1="", p2=2)) */
        bool kwarg;

        /* Export as a method or like a normal(static) function? (used by TypeInit) */
        bool method;
    };

#define ARGON_FUNCTION(name, doc, arity, variadic, kw)                                                  \
ArObject *name##_fn(ArObject *_func, ArObject *_self, ArObject **args, ArObject *kwargs, ArSize argc);  \
FunctionDef name = {#name, doc, name##_fn, arity, variadic, kw, false};                                 \
ArObject *name##_fn(ArObject *_func, ArObject *_self, ArObject **args, ArObject *kwargs, ArSize argc)

#define ARGON_METHOD(name, doc, arity, variadic, kw)                                                    \
ArObject *name##_fn(ArObject *_func, ArObject *_self, ArObject **args, ArObject *kwargs, ArSize argc);  \
FunctionDef name = {#name, doc, name##_fn, arity, variadic, kw, true};                                  \
ArObject *name##_fn(ArObject *_func, ArObject *_self, ArObject **args, ArObject *kwargs, ArSize argc)

} // namespace argon::vm::datatype

ENUMBITMASK_ENABLE(argon::vm::datatype::BufferFlags);

ENUMBITMASK_ENABLE(argon::vm::datatype::TypeInfoFlags);

#endif // !ARGON_VM_DATATYPE_OBJECTDEF_H_
