// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_OBJECTDEF_H_
#define ARGON_VM_DATATYPE_OBJECTDEF_H_

#include <atomic>
#include <cassert>
#include <cstddef>

#include <argon/vm/memory/refcount.h>
#include <argon/vm/sync/notifyqueue.h>

#include <argon/util/enum_bitmask.h>

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
    return nullptr                              \

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
    using AttributeGetter = struct ArObject *(*)(const struct ArObject *, struct ArObject *, bool static_attr);
    using AttributeWriter = bool (*)(struct ArObject *, struct ArObject *, struct ArObject *, bool static_attr);
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

        /* C-String describing the parameters that the function accepts as input. */
        const char *params;

        /* Is a variadic function? (func variadic(p1,p2,...p3)) */
        bool variadic;

        /* Can it accept keyword parameters?? (func kwargs(p1="", p2=2)) */
        bool kwarg;

        /* Export as a method or like a normal(static) function? (used by TypeInit) */
        bool method;
    };

#define ARGON_FUNCTION(name, exported_name, doc, params, variadic, kw)                                  \
ArObject *name##_fn(ArObject *_func, ArObject *_self, ArObject **args, ArObject *kwargs, ArSize argc);  \
const FunctionDef name = {#exported_name, doc, name##_fn, params, variadic, kw, false};                 \
ArObject *name##_fn(ArObject *_func, ArObject *_self, ArObject **args, ArObject *kwargs, ArSize argc)

#define ARGON_METHOD(name, exported_name, doc, params, variadic, kw)                                    \
ArObject *name##_fn(ArObject *_func, ArObject *_self, ArObject **args, ArObject *kwargs, ArSize argc);  \
const FunctionDef name = {#exported_name, doc, name##_fn, params, variadic, kw, true};                  \
ArObject *name##_fn(ArObject *_func, ArObject *_self, ArObject **args, ArObject *kwargs, ArSize argc)

#define ARGON_METHOD_INHERITED(name, exported_name)                                                     \
ArObject *name##_fn(ArObject *_func, ArObject *_self, ArObject **args, ArObject *kwargs, ArSize argc);  \
const FunctionDef name = {#exported_name, nullptr, name##_fn, nullptr, false, false, true};             \
ArObject *name##_fn(ArObject *_func, ArObject *_self, ArObject **args, ArObject *kwargs, ArSize argc)

#define ARGON_METHOD_STUB(name, doc, params, variadic, kw)  {name, doc, nullptr, params, variadic, kw, true}

#define ARGON_METHOD_SENTINEL {nullptr, nullptr, nullptr, 0, false, false, false}

    using MemberGetFn = ArObject *(*)(const ArObject *);
    using MemberSetFn = bool (*)(const ArObject *, ArObject *value);

    enum class MemberType {
        BOOL,
        DOUBLE,
        FLOAT,
        INT,
        LONG,
        OBJECT,
        SHORT,
        STRING,
        UINT,
        ULONG,
        USHORT
    };

    struct MemberDef {
        const char *name;

        MemberGetFn get;
        MemberSetFn set;

        MemberType type;

        int offset;
        bool readonly;
    };

    struct Monitor {
        vm::sync::NotifyQueue w_queue;

        std::atomic_uintptr_t a_fiber;

        unsigned int locks;
    };

#define ARGON_MEMBER(name, type, offset, readonly) {name, nullptr, nullptr, type, offset, readonly}
#define ARGON_MEMBER_GETSET(name, get, set) {name, get, set, MemberType::ULONG, 0, false}
#define ARGON_MEMBER_SENTINEL {nullptr, nullptr, nullptr, MemberType::ULONG, 0, false}

#define AROBJ_HEAD                                                      \
    struct {                                                            \
        argon::vm::memory::RefCount ref_count_;                         \
        const struct argon::vm::datatype::TypeInfo *type_;              \
        std::atomic<struct argon::vm::datatype::Monitor *> mon_;        \
    } head_

#define AROBJ_HEAD_INIT(type) {                                         \
        argon::vm::memory::RefCount(argon::vm::memory::RCType::STATIC), \
        (type),                                                         \
        nullptr}

#define AROBJ_HEAD_INIT_TYPE AROBJ_HEAD_INIT(argon::vm::datatype::type_type_)

    /**
     * @brief Allows you to use the datatype as if it were a buffer.
     */
    struct BufferSlots {
        BufferGetFn get_buffer;
        BufferRelFn rel_buffer;
    };

    /**
     * @brief Allows you to use the datatype as if it were a number in contexts that require it (e.g. slice).
     */
    struct NumberSlots {
        UnaryOp as_index;
        UnaryOp as_integer;
    };

    /**
     * @brief Models the behavior of the datatype when used as an object (e.g. mytype.property).
     */
    struct ObjectSlots {
        const FunctionDef *methods;
        const MemberDef *members;

        struct TypeInfo **traits;

        AttributeGetter get_attr;
        AttributeWriter set_attr;

        int namespace_offset;
    };

    /**
     * @brief Model the behavior of the datatype with the common operations (e.g. +, -, /, *).
     */
    struct OpSlots {
        // Math
        BinaryOp add;
        BinaryOp sub;
        BinaryOp mul;
        BinaryOp div;
        BinaryOp idiv;
        BinaryOp mod;
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
        UnaryOp inc;
        UnaryOp dec;
    };

#define AR_GET_BINARY_OP(struct, offset) *((BinaryOp *) (((unsigned char *) (struct)) + (offset)))

    /**
     * @brief Models the behavior of the datatype that supports the subscript [] operator (e.g. list, dict, tuple).
     */
    struct SubscriptSlots {
        ArSize_UnaryOp length;
        BinaryOp get_item;
        Bool_TernaryOp set_item;
        BinaryOp get_slice;
        Bool_TernaryOp set_slice;
        BinaryOp item_in;
    };

    /**
     * @brief An Argon type is represented by this structure.
     */
    struct TypeInfo {
        AROBJ_HEAD;

        /// Datatype name
        const char *name;

        /// An optional qualified name for datatype.
        const char *qname;

        /// An optional datatype documentation.
        const char *doc;

        /// Size of the object represented by this datatype (used for memory allocation).
        const unsigned int size;

        /// Datatype flags (change the behavior of the datatype under certain circumstances).
        const TypeInfoFlags flags;

        /// Datatype constructor.
        VariadicOp ctor;

        /// Datatype destructor.
        Bool_UnaryOp dtor;

        /// GC trace.
        TraceOp trace;

        /// Pointer to a function that implements datatype hashing.
        ArSize_UnaryOp hash;

        /// An optional pointer to function that returns datatype truthiness (if nullptr, the default is true).
        Bool_UnaryOp is_true;

        /// An optional pointer to function that make this datatype comparable.
        CompareOp compare;

        /// An optional pointer to function that returns the string representation.
        UnaryConstOp repr;

        /// An optional pointer to function that returns the string conversion.
        UnaryOp str;

        /// An optional pointer to function that returns datatype iterator.
        UnaryBoolOp iter;

        /// An optional pointer to function that returns next element.
        UnaryOp iter_next;

        /// Pointer to BufferSlots structure relevant only if the object implements bufferable behavior.
        const BufferSlots *buffer;

        /// Pointer to NumberSlots structure relevant only if the object implements numeric behavior.
        const NumberSlots *number;

        /// Pointer to ObjectSlots structure relevant only if the object implements instance like behavior.
        const ObjectSlots *object;

        /// Pointer to SubscriptSlots structure relevant only if the object implements "container" behavior.
        const SubscriptSlots *subscriptable;

        /// Pointer to OpSlots structure that contains the common operations for an object.
        const OpSlots *ops;

        ArObject *mro;

        ArObject *tp_map;
    };

    struct ArObject {
        AROBJ_HEAD;
    };

#define AR_GET_HEAD(object)                 ((object)->head_)
#define AR_GET_RC(object)                   (AR_GET_HEAD(object).ref_count_)
#define AR_UNSAFE_GET_RC(object)            (*((ArSize *) &AR_GET_HEAD(object).ref_count_))
#define AR_GET_TYPE(object)                 (AR_GET_HEAD(object).type_)
#define AR_GET_MON(object)                  (AR_GET_HEAD(object).mon_)
#define AR_UNSAFE_GET_MON(object)           (*((Monitor **) &AR_GET_HEAD(object).mon_))

#define AR_SLOT_BUFFER(object)              ((AR_GET_TYPE(object))->buffer)
#define AR_SLOT_NUMBER(object)              ((AR_GET_TYPE(object))->number)
#define AR_SLOT_OBJECT(_object)             ((AR_GET_TYPE(_object))->object)
#define AR_SLOT_SUBSCRIPTABLE(object)       ((AR_GET_TYPE(object))->subscriptable)

#define AR_ISITERABLE(object)               (AR_GET_TYPE(object)->iter != nullptr)
#define AR_ISSUBSCRIPTABLE(object)          (AR_SLOT_SUBSCRIPTABLE(object) != nullptr)
#define AR_HAVE_OBJECT_BEHAVIOUR(_object)   (AR_SLOT_OBJECT(_object) != nullptr)

#define AR_SAME_TYPE(object, other)         (AR_GET_TYPE(object) == AR_GET_TYPE(other))
#define AR_TYPE_NAME(object)                (AR_GET_TYPE(object)->name)
#define AR_TYPE_QNAME(object)               (AR_GET_TYPE(object)->qname)
#define AR_TYPEOF(object, type)             (AR_GET_TYPE(object) == (type))

#define AR_GET_NSOFFSET(object)             (AR_HAVE_OBJECT_BEHAVIOUR(object) && AR_SLOT_OBJECT(object)->namespace_offset >= 0 ?  \
    ((ArObject **) (((unsigned char *) (object)) + AR_SLOT_OBJECT(object)->namespace_offset)) : nullptr)
} // namespace argon::vm::datatype

ENUMBITMASK_ENABLE(argon::vm::datatype::BufferFlags);

ENUMBITMASK_ENABLE(argon::vm::datatype::TypeInfoFlags);

#endif // !ARGON_VM_DATATYPE_OBJECTDEF_H_
