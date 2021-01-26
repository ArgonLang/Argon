// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_AROBJECT_H_
#define ARGON_OBJECT_AROBJECT_H_

#include <unistd.h>

#include <utils/enum_bitmask.h>

#include "refcount.h"

namespace argon::object {
    enum class ArBufferFlags {
        READ = 0x01,
        WRITE = 0x02
    };

    enum class CompareMode : unsigned char {
        EQ,
        NE,
        GE,
        GEQ,
        LE,
        LEQ
    };

    using ArSize = size_t;
    using ArSSize = ssize_t;

    using ArSizeUnaryOp = ArSSize (*)(struct ArObject *);
    using BinaryOp = struct ArObject *(*)(struct ArObject *, struct ArObject *);
    using BinaryOpArSize = struct ArObject *(*)(struct ArObject *, ArSSize);
    using BoolBinOp = bool (*)(struct ArObject *, struct ArObject *);
    using BoolTernOp = bool (*)(struct ArObject *, struct ArObject *, struct ArObject *);
    using BoolTernOpArSize = bool (*)(struct ArObject *, struct ArObject *, ArSSize);
    using BoolUnaryOp = bool (*)(struct ArObject *obj);
    using CompareOp = struct ArObject *(*)(struct ArObject *, struct ArObject *, CompareMode);
    using SizeTUnaryOp = ArSize (*)(struct ArObject *);
    using VariadicOp = struct ArObject *(*)(struct ArObject **, ArSize);
    using VoidUnaryOp = void (*)(struct ArObject *obj);
    using UnaryOp = struct ArObject *(*)(struct ArObject *);

    struct ArBuffer {
        unsigned char *buffer;
        ArSize len;

        ArObject *obj;
        ArBufferFlags flags;
    };

    using NativeFuncPtr = ArObject *(*)(ArObject *self, ArObject **argv, ArSize count);
    struct NativeFunc {
        /* Name of native function (this name will be exposed to Argon) */
        const char *name;

        /* Documentation of native function (this doc will be exposed to Argon) */
        const char *doc;

        /* Pointer to native code */
        NativeFuncPtr func;

        /* Arity of the function, how many args accepts in input?! */
        unsigned short arity;

        /* Is a variadic function? (func variadic(p1,p2,...p3))*/
        bool variadic;
    };

    using BufferGetFn = bool (*)(struct ArObject *obj, ArBuffer *buffer, ArBufferFlags flags);
    using BufferRelFn = void (*)(ArBuffer *buffer);
    struct BufferSlots {
        BufferGetFn get_buffer;
        BufferRelFn rel_buffer;
    };

    struct IteratorSlots {
        BoolUnaryOp has_next;
        UnaryOp next;
        UnaryOp peek;
        VoidUnaryOp reset;
    };

    struct MapSlots {
        SizeTUnaryOp length;
        BinaryOp get_item;
        BoolTernOp set_item;
    };

    struct NumberSlots {
        UnaryOp as_integer;
        ArSizeUnaryOp as_index;
    };

    struct ObjectSlots {
        const NativeFunc *methods;

        BinaryOp get_attr;
        BinaryOp get_static_attr;
        BoolTernOp set_attr;
        BoolTernOp set_static_attr;
    };

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

    struct SequenceSlots {
        SizeTUnaryOp length;
        BinaryOpArSize get_item;
        BoolTernOpArSize set_item;
        BinaryOp get_slice;
        BoolTernOp set_slice;
    };

    struct ArObject {
        RefCount ref_count;
        const struct TypeInfo *type;
    };

    using Trace = void (*)(struct ArObject *, VoidUnaryOp);
    struct TypeInfo : ArObject {
        const char *name;
        const char *doc;
        const unsigned short size;

        /* Datatype constructor */
        VariadicOp ctor;

        /* Datatype destructor */
        VoidUnaryOp cleanup;

        /* GC trace method */
        Trace trace;

        /* Make this datatype comparable */
        CompareOp compare;

        /* Make this datatype comparable for equality */
        BoolBinOp equal;

        /* Return datatype truthiness */
        BoolUnaryOp is_true;

        /* Return datatype hash */
        SizeTUnaryOp hash;

        /* Return string datatype representation */
        UnaryOp str;

        /* Returns an iterator for this datatype */
        UnaryOp iter_get;

        /* Returns a reverse iterator for this datatype */
        UnaryOp iter_rget;

        /* Pointer to BufferSlots structure relevant only if the object implements bufferable behavior */
        const BufferSlots *buffer_actions;

        /* Pointer to IteratorSlots structure relevant only if this datatype is an iterator */
        const IteratorSlots *iterator_actions;

        /* Pointer to MapSlots structure relevant only if the object implements mapping behavior */
        const MapSlots *map_actions;

        /* Pointer to NumberSlots structure relevant only if the object implements numeric behavior */
        const NumberSlots *number_actions;

        /* Pointer to ObjectSlots structure relevant only if the object implements instance like behavior */
        const ObjectSlots *obj_actions;

        /* Pointer to SequenceSlots structure relevant only if the object implements sequence behavior */
        const SequenceSlots *sequence_actions;

        /* Pointer to OpSlots structure that contains the common operations for an object */
        const OpSlots *ops;

        /* Pointer to dynamically allocated namespace that contains relevant type methods (if any, nullptr otherwise) */
        ArObject *tp_map;
    };

    extern const TypeInfo type_type_;

#define TYPEINFO_STATIC_INIT        {{RefCount(RCType::STATIC)}, &type_type_}
#define AR_GET_TYPE(object)         ((object)->type)
#define AR_TYPEOF(object, type)     (AR_GET_TYPE(object) == &(type))
#define AR_SAME_TYPE(object, other) (AR_GET_TYPE(object) == AR_GET_TYPE(other))
#define AR_EQUAL(object, other)     (AR_GET_TYPE(object)->equal(object, other))
#define AR_TYPE_NAME(object)        (AR_GET_TYPE(object)->name)
#define AR_ITERATOR_SLOT(object)    (AR_GET_TYPE(object)->iterator_actions)
#define AR_MAP_SLOT(object)         (AR_GET_TYPE(object)->map_actions)
#define AR_NUMBER_SLOT(object)      (AR_GET_TYPE(object)->number_actions)
#define AR_OBJECT_SLOT(object)      (AR_GET_TYPE(object)->obj_actions)
#define AR_SEQUENCE_SLOT(object)    (AR_GET_TYPE(object)->sequence_actions)

    ArObject *ArObjectGCNew(const TypeInfo *type);

    ArObject *ArObjectNew(RCType rc, const TypeInfo *type);

    template<typename T>
    inline typename std::enable_if<std::is_base_of<ArObject, T>::value, T>::type *
    ArObjectGCNew(const TypeInfo *type) {
        return (T *) ArObjectGCNew(type);
    }

    template<typename T>
    inline typename std::enable_if<std::is_base_of<ArObject, T>::value, T>::type *
    ArObjectNew(RCType rc, const TypeInfo *type) {
        return (T *) ArObjectNew(rc, type);
    }

    ArObject *IteratorGet(const ArObject *obj);

    ArObject *IteratorGetReversed(const ArObject *obj);

    ArObject *IteratorNext(ArObject *iterator);

    ArObject *ToString(ArObject *obj);

    template<typename T>
    inline typename std::enable_if<std::is_base_of<ArObject, T>::value, T>::type *IncRef(T *obj) {
        if (obj != nullptr)
            obj->ref_count.IncStrong();
        return obj;
    }

    ArSize Hash(ArObject *obj);

    ArSSize Length(const ArObject *obj);

    inline bool AsIndex(const ArObject *obj) {
        return AR_GET_TYPE(obj)->number_actions != nullptr
               && AR_GET_TYPE(obj)->number_actions->as_index;
    }

    inline bool AsInteger(const ArObject *obj) {
        return AR_GET_TYPE(obj)->number_actions != nullptr
               && AR_GET_TYPE(obj)->number_actions->as_integer != nullptr;
    }

    inline bool AsMap(const ArObject *obj) { return AR_GET_TYPE(obj)->map_actions != nullptr; }

    inline bool AsNumber(const ArObject *obj) { return AR_GET_TYPE(obj)->number_actions != nullptr; }

    inline bool AsSequence(const ArObject *obj) { return AR_GET_TYPE(obj)->sequence_actions != nullptr; }

    inline bool IsHashable(const ArObject *obj) { return AR_GET_TYPE(obj)->hash != nullptr; }

    inline bool IsBufferable(const ArObject *obj) { return AR_GET_TYPE(obj)->buffer_actions != nullptr; }

    inline bool IsIterable(const ArObject *obj) { return AR_GET_TYPE(obj)->iter_get != nullptr; }

    inline bool IsIterableReversed(const ArObject *obj) { return AR_GET_TYPE(obj)->iter_rget != nullptr; }

    inline bool IsIterator(const ArObject *obj) { return AR_GET_TYPE(obj)->iterator_actions != nullptr; }

    inline bool IsTrue(const ArObject *obj) { return AR_GET_TYPE(obj)->is_true((ArObject *) obj); }

    bool TypeInit(TypeInfo *info);

    bool BufferGet(ArObject *obj, ArBuffer *buffer, ArBufferFlags flags);

    bool BufferSimpleFill(ArObject *obj, ArBuffer *buffer, ArBufferFlags flags, unsigned char *raw, ArSize len,
                          bool writable);

    bool VariadicCheckPositional(const char *name, int nargs, int min, int max);

    void BufferRelease(ArBuffer *buffer);

    void Release(ArObject *obj);

    inline void Release(ArObject **obj) {
        Release(*obj);
        *obj = nullptr;
    }

} // namespace argon::object

ENUMBITMASK_ENABLE(argon::object::ArBufferFlags);

#endif // !ARGON_OBJECT_AROBJECT_H_
