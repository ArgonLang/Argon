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

    using ArSize_UnaryOp = ArSize (*)(const struct ArObject *);
    using BinaryOp = struct ArObject *(*)(struct ArObject *, struct ArObject *);
    using Bool_TernaryOp = bool (*)(struct ArObject *, struct ArObject *, struct ArObject *);
    using Bool_UnaryOp = bool (*)(const struct ArObject *);
    using CompareOp = struct ArObject *(*)(struct ArObject *, struct ArObject *, CompareMode);
    using UnaryOp = struct ArObject *(*)(struct ArObject *);
    using UnaryBoolOp = struct ArObject *(*)(struct ArObject *, bool);
    using VariadicOp = struct ArObject *(*)(const struct TypeInfo *, ArObject **, ArSize);
    using Void_UnaryOp = void (*)(struct ArObject *);

    using TraceOp = void (*)(struct ArObject *, Void_UnaryOp);

#define AROBJ_HEAD                      \
    struct {                            \
        memory::RefCount ref_count_;    \
        const struct TypeInfo *type_;   \
    } head_

#define AROBJ_HEAD_INIT(type) {                                         \
        argon::vm::memory::RefCount(argon::vm::memory::RCType::STATIC), \
        (type) }

#define AROBJ_HEAD_INIT_TYPE AROBJ_HEAD_INIT(nullptr)

    enum class TypeInfoFlags : unsigned int {
        BASE
    };

    /**
     * @brief Allows you to use the datatype as if it were a buffer.
     */
    struct BufferSlots {

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
        BinaryOp inp_mul;
        BinaryOp inp_div;
        UnaryOp inc;
        UnaryOp dec;
    };

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
        VariadicOp dtor;

        /// GC trace.
        TraceOp trace;

        /// Pointer to a function that implements datatype hashing.
        ArSize_UnaryOp hash;

        /// An optional pointer to function that returns datatype truthiness (if nullptr, the default is true).
        Bool_UnaryOp is_true;

        /// An optional pointer to function that make this datatype comparable.
        CompareOp compare;

        /// An optional pointer to function that returns the string representation.
        UnaryOp repr;

        /// An optional pointer to function that returns the string conversion.
        UnaryOp str;

        /// An optional pointer to function that returns datatype iterator.
        UnaryBoolOp iter;

        /// An optional pointer to function that returns next element.
        UnaryOp iter_next;

        /// Pointer to BufferSlots structure relevant only if the object implements bufferable behavior.
        BufferSlots *buffer;

        /// Pointer to NumberSlots structure relevant only if the object implements numeric behavior.
        NumberSlots *number;

        /// Pointer to ObjectSlots structure relevant only if the object implements instance like behavior.
        ObjectSlots *object;

        /// Pointer to SubscriptSlots structure relevant only if the object implements "container" behavior.
        SubscriptSlots *subscriptable;

        /// Pointer to OpSlots structure that contains the common operations for an object.
        OpSlots *ops;

        ArObject *_t1;

        ArObject *_t2;
    };

#define AR_GET_RC(object)           ((object)->head_.ref_count_)
#define AR_GET_TYPE(object)         ((object)->type_)
#define AR_SAME_TYPE(object, other) (AR_GET_TYPE(object) == AR_GET_TYPE(other))
#define AR_TYPE_NAME(object)        (AR_GET_TYPE(object)->name)
#define AR_TYPEOF(object, type)     (AR_GET_TYPE(object) == (type))

    struct ArObject {
        AROBJ_HEAD;
    };

    ArObject *Repr(const ArObject *object);

    ArObject *Str(const ArObject *object);

    template<typename T>
    T *IncRef(T *t) {
        if (t != nullptr && !AR_GET_RC(t).IncStrong())
            return nullptr;

        return t;
    }

    void Release(ArObject *object);

    inline void Release(ArObject **object) {
        Release(*object);
        *object = nullptr;
    }
} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_AROBJECT_H_
