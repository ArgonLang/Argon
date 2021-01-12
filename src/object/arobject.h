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
    using VoidUnaryOp = void (*)(struct ArObject *obj);
    using UnaryOp = struct ArObject *(*)(struct ArObject *);

    struct ArBuffer {
        unsigned char *buffer;
        ArSize len;

        ArObject *obj;
        ArBufferFlags flags;
    };

    using BufferGetFn = bool (*)(struct ArObject *obj, ArBuffer *buffer, ArBufferFlags flags);
    using BufferRelFn = void (*)(ArBuffer *buffer);
    struct BufferSlots {
        BufferGetFn get_buffer;
        BufferRelFn rel_buffer;
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

        /* Object constructor */
        UnaryOp ctor;

        /* Object destructor */
        VoidUnaryOp cleanup;

        /* GC trace method */
        Trace trace;

        /* Make this object comparable */
        CompareOp compare;

        /* Make this object comparable for equality */
        BoolBinOp equal;

        /* Return object truthiness */
        BoolUnaryOp is_true;

        /* Return object hash */
        SizeTUnaryOp hash;

        /* Return string object representation */
        UnaryOp str;

        /* Pointer to BufferSlots structure relevant only if the object implements bufferable behavior */
        const BufferSlots *buffer_actions;

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
    };

    extern const TypeInfo type_type_;

#define TYPEINFO_STATIC_INIT        {{RefCount(RCType::STATIC)}, &type_type_}
#define AR_GET_TYPE(object)         ((object)->type)
#define AR_TYPEOF(object, type)     (AR_GET_TYPE(object) == &(type))
#define AR_SAME_TYPE(object, other) (AR_GET_TYPE(object) == AR_GET_TYPE(other))
#define AR_EQUAL(object, other)     (AR_GET_TYPE(object)->equal(object, other))
#define AR_TYPE_NAME(object)        (AR_GET_TYPE(object)->name)

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

    // Management functions

    inline bool AsIndex(const ArObject *obj) {
        return obj->type->number_actions != nullptr && obj->type->number_actions->as_index;
    }

    bool BufferGet(ArObject *obj, ArBuffer *buffer, ArBufferFlags flags);

    void BufferRelease(ArBuffer *buffer);

    bool BufferSimpleFill(ArObject *obj, ArBuffer *buffer, ArBufferFlags flags, unsigned char *raw, ArSize len,
                          bool writable);

    ArSize Hash(ArObject *obj);

    bool IsHashable(ArObject *obj);

    ArObject *ToString(ArObject *obj);

    inline void IncRef(ArObject *obj) {
        if (obj != nullptr)
            obj->ref_count.IncStrong();
    }

    inline bool IsBufferable(const ArObject *obj) { return obj->type->buffer_actions != nullptr; }

    inline bool IsMap(const ArObject *obj) { return obj->type->map_actions != nullptr; }

    inline bool IsNumber(const ArObject *obj) { return obj->type->number_actions != nullptr; }

    inline bool IsSequence(const ArObject *obj) { return obj->type->sequence_actions != nullptr; }

    bool IsTrue(const ArObject *obj);

    void Release(ArObject *obj);

    inline void Release(ArObject **obj) {
        Release(*obj);
        *obj = nullptr;
    }

} // namespace argon::object

ENUMBITMASK_ENABLE(argon::object::ArBufferFlags);

#endif // !ARGON_OBJECT_AROBJECT_H_
