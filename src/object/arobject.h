// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_AROBJECT_H_
#define ARGON_OBJECT_AROBJECT_H_

#include <utils/enum_bitmask.h>

#include "refcount.h"

namespace argon::object {
    enum class ArBufferFlags {
        READ = 0x01,
        WRITE = 0x02
    };

    enum class CompareMode : unsigned char {
        EQ = 0,
        NE = 1,
        GE = 2,
        GEQ = 3,
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
        case CompareMode::GE:                   \
            return BoolToArBool((a) > (b));     \
        case CompareMode::GEQ:                  \
            return BoolToArBool((a) >= (b));    \
        case CompareMode::LE:                   \
            return BoolToArBool((a) < (b));     \
        case CompareMode::LEQ:                  \
            return BoolToArBool((a) <= (b));    \
    }                                           \
    assert(false)

    using ArSize = size_t;
    using ArSSize = long int;

    using ArSizeUnaryOp = ArSSize (*)(struct ArObject *);
    using BinaryOp = struct ArObject *(*)(struct ArObject *, struct ArObject *);
    using BinaryOpArSize = struct ArObject *(*)(struct ArObject *, ArSSize);
    using BoolTernOp = bool (*)(struct ArObject *, struct ArObject *, struct ArObject *);
    using BoolTernOpArSize = bool (*)(struct ArObject *, struct ArObject *, ArSSize);
    using BoolUnaryOp = bool (*)(struct ArObject *obj);
    using CompareOp = struct ArObject *(*)(struct ArObject *, struct ArObject *, CompareMode);
    using SizeTUnaryOp = ArSize (*)(struct ArObject *);
    using VariadicOp = struct ArObject *(*)(const struct TypeInfo *type, struct ArObject **, ArSize);
    using VoidUnaryOp = void (*)(struct ArObject *obj);
    using UnaryOp = struct ArObject *(*)(struct ArObject *);

    struct ArBuffer {
        unsigned char *buffer;
        ArSize len;

        ArObject *obj;
        ArBufferFlags flags;
    };

    using NativeFuncPtr = ArObject *(*)(ArObject *func, ArObject *self, ArObject **argv, ArSize count);
    struct NativeFunc {
        /* Name of native function (this name will be exposed to Argon) */
        const char *name;

        /* Documentation of native function (this doc will be exposed to Argon) */
        const char *doc;

        /* Pointer to native code */
        NativeFuncPtr func;

        /* Arity of the function, how many args accepts in input?! */
        unsigned short arity;

        /* Is a variadic function? (func variadic(p1,p2,...p3)) */
        bool variadic;

        /* Export as a method or like a normal(static) function? (used by TypeInit) */
        bool method;
    };

#define ARGON_FUNCTION5(prefix, name, doc, arity, variadic)                                 \
ArObject *prefix##name##_fn(ArObject *func, ArObject *self, ArObject **argv, ArSize count); \
NativeFunc prefix##name##_ = {#name, doc, prefix##name##_fn, arity, variadic, false};       \
ArObject *prefix##name##_fn(ArObject *func, ArObject *self, ArObject **argv, ArSize count)

#define ARGON_METHOD5(prefix, name, doc, arity, variadic)                                   \
ArObject *prefix##name##_fn(ArObject *func, ArObject *self, ArObject **argv, ArSize count); \
NativeFunc prefix##name##_ = {#name, doc, prefix##name##_fn,  (arity)+1, variadic, true};   \
ArObject *prefix##name##_fn(ArObject *func, ArObject *self, ArObject **argv, ArSize count)

#define ARGON_FUNCTION(name, doc, arity, variadic)          ARGON_FUNCTION5(,name, doc, arity, variadic)
#define ARGON_METHOD(name, doc, arity, variadic)            ARGON_FUNCTION5(,name, doc, arity, variadic)

#define ARGON_CALL_FUNC5(prefix, name, origin, self, argv, count)   prefix##name##_fn(origin, self, argv, count)
#define ARGON_CALL_FUNC(name, origin, self, argv, count)            ARGON_CALL_FUNC5(,name,origin,self,argv,count)

#define ARGON_METHOD_SENTINEL   {nullptr, nullptr, nullptr, 0, false}

    enum class NativeMemberType {
        AROBJECT,
        DOUBLE,
        FLOAT,
        INT,
        LONG,
        SHORT,
        STRING
    };

    struct NativeMember {
        const char *name;
        NativeMemberType type;
        int offset;
        bool readonly;
    };

#define ARGON_MEMBER_SENTINEL {nullptr, (NativeMemberType)0, 0, false}

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
        const NativeMember *members;
        const TypeInfo **traits;

        BinaryOp get_attr;
        BinaryOp get_static_attr;
        BoolTernOp set_attr;
        BoolTernOp set_static_attr;

        int nsoffset;
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

    enum class TypeInfoFlags {
        BASE,
        STRUCT,
        TRAIT
    };

    using Trace = void (*)(struct ArObject *, VoidUnaryOp);
    struct TypeInfo : ArObject {
        const char *name;
        const char *doc;
        const unsigned short size;

        /* Type flags */
        TypeInfoFlags flags;

        /* Datatype constructor */
        VariadicOp ctor;

        /* Datatype destructor */
        VoidUnaryOp cleanup;

        /* GC trace method */
        Trace trace;

        /* Make this datatype comparable */
        CompareOp compare;

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

        /* Pointer to dynamically allocated tuple that contains Trait & Struct method resolution order */
        ArObject *mro;

        /* Pointer to dynamically allocated namespace that contains relevant type methods (if any, nullptr otherwise) */
        ArObject *tp_map;
    };

    extern const TypeInfo *type_type_;

    extern const TypeInfo *type_trait_;

#define TYPEINFO_STATIC_INIT        {{RefCount(RCType::STATIC)}, type_type_}
#define AR_GET_TYPE(object)         ((object)->type)
#define AR_GET_TYPEOBJ(object)      (AR_TYPEOF(object, type_type_) ? (const TypeInfo *) (object) : AR_GET_TYPE(object))
#define AR_TYPEOF(object, type)     (AR_GET_TYPE(object) == (type))
#define AR_SAME_TYPE(object, other) (AR_GET_TYPE(object) == AR_GET_TYPE(other))
#define AR_TYPE_NAME(object)        (AR_GET_TYPE(object)->name)
#define AR_IS_TYPE_INSTANCE(object) (AR_GET_TYPE(object) != type_type_)
#define AR_ITERATOR_SLOT(object)    (AR_GET_TYPE(object)->iterator_actions)
#define AR_MAP_SLOT(object)         (AR_GET_TYPE(object)->map_actions)
#define AR_NUMBER_SLOT(object)      (AR_GET_TYPE(object)->number_actions)
#define AR_OBJECT_SLOT(object)      (AR_GET_TYPE(object)->obj_actions)

#define AR_GET_NSOFF(obj)           (AR_OBJECT_SLOT(obj) != nullptr ? \
    ((ArObject **) (((unsigned char *) (obj)) + AR_OBJECT_SLOT(obj)->nsoffset)) : nullptr)

#define AR_SEQUENCE_SLOT(object)    (AR_GET_TYPE(object)->sequence_actions)

    ArObject *ArObjectGCNew(const TypeInfo *type);

    ArObject *ArObjectGCNewTrack(const TypeInfo *type);

    ArObject *ArObjectNew(RCType rc, const TypeInfo *type);

    template<typename T>
    inline typename std::enable_if<std::is_base_of<ArObject, T>::value, T>::type *
    ArObjectGCNew(const TypeInfo *type) {
        return (T *) ArObjectGCNew(type);
    }

    template<typename T>
    inline typename std::enable_if<std::is_base_of<ArObject, T>::value, T>::type *
    ArObjectGCNewTrack(const TypeInfo *type) {
        return (T *) ArObjectGCNewTrack(type);
    }

    template<typename T>
    inline typename std::enable_if<std::is_base_of<ArObject, T>::value, T>::type *
    ArObjectNew(RCType rc, const TypeInfo *type) {
        return (T *) ArObjectNew(rc, type);
    }

    ArObject *InstanceGetMethod(const ArObject *instance, const ArObject *key, bool *is_meth);

    ArObject *InstanceGetMethod(const ArObject *instance, const char *key, bool *is_meth);

    ArObject *IteratorGet(const ArObject *obj);

    ArObject *IteratorGetReversed(const ArObject *obj);

    ArObject *IteratorNext(ArObject *iterator);

    ArObject *PropertyGet(const ArObject *obj, const ArObject *key, bool instance);

    ArObject *RichCompare(const ArObject *obj, const ArObject *other, CompareMode mode);

    ArObject *ToString(ArObject *obj);

    ArObject *TypeNew(const TypeInfo *meta, const char *name, ArObject *ns, TypeInfo **bases, ArSize count);

    ArObject *TypeNew(const TypeInfo *meta, ArObject *name, ArObject *ns, TypeInfo **bases, ArSize count);

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

    bool Equal(const ArObject *obj, const ArObject *other);

    inline bool IsHashable(const ArObject *obj) { return AR_GET_TYPE(obj)->hash != nullptr; }

    inline bool IsBufferable(const ArObject *obj) { return AR_GET_TYPE(obj)->buffer_actions != nullptr; }

    inline bool IsIterable(const ArObject *obj) { return AR_GET_TYPE(obj)->iter_get != nullptr; }

    inline bool IsIterableReversed(const ArObject *obj) { return AR_GET_TYPE(obj)->iter_rget != nullptr; }

    inline bool IsIterator(const ArObject *obj) { return AR_GET_TYPE(obj)->iterator_actions != nullptr; }

    bool IsNull(const ArObject *obj);

    bool IsTrue(const ArObject *obj);

    bool PropertySet(ArObject *obj, ArObject *key, ArObject *value, bool member);

    bool TraitIsImplemented(const ArObject *obj, const TypeInfo *type);

    bool TypeInit(TypeInfo *info, ArObject *ns);

    bool BufferGet(ArObject *obj, ArBuffer *buffer, ArBufferFlags flags);

    bool BufferSimpleFill(ArObject *obj, ArBuffer *buffer, ArBufferFlags flags, unsigned char *raw, ArSize len,
                          bool writable);

    bool VariadicCheckPositional(const char *name, int nargs, int min, int max);

    int TrackRecursive(ArObject *obj);

    void UntrackRecursive(ArObject *obj);

    void BufferRelease(ArBuffer *buffer);

    void Release(ArObject *obj);

    inline void Release(ArObject **obj) {
        Release(*obj);
        *obj = nullptr;
    }

} // namespace argon::object

ENUMBITMASK_ENABLE(argon::object::ArBufferFlags);

#endif // !ARGON_OBJECT_AROBJECT_H_
