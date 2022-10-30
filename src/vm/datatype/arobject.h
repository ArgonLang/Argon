// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_AROBJECT_H_
#define ARGON_VM_DATATYPE_AROBJECT_H_

#include <cstddef>

#include <vm/memory/refcount.h>

#include "objectdef.h"

namespace argon::vm::datatype {
#define AROBJ_HEAD                                                      \
    struct {                                                            \
        argon::vm::memory::RefCount ref_count_;                         \
        const struct argon::vm::datatype::TypeInfo *type_;              \
    } head_

#define AROBJ_HEAD_INIT(type) {                                         \
        argon::vm::memory::RefCount(argon::vm::memory::RCType::STATIC), \
        (type) }

#define AROBJ_HEAD_INIT_TYPE AROBJ_HEAD_INIT(nullptr)

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
        const struct TypeInfo **traits;
        const FunctionDef *methods;
        const void *_stub;

        int nsoff;
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
        UnaryConstOp str;

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
        const OpSlots *ops;

        ArObject *_t1;

        ArObject *tp_map;
    };

#define AR_GET_RC(object)           ((object)->head_.ref_count_)
#define AR_GET_TYPE(object)         ((object)->head_.type_)
#define AR_ISITERABLE(object)       (AR_GET_TYPE(object)->iter != nullptr)
#define AR_SAME_TYPE(object, other) (AR_GET_TYPE(object) == AR_GET_TYPE(other))
#define AR_TYPE_NAME(object)        (AR_GET_TYPE(object)->name)
#define AR_TYPEOF(object, type)     (AR_GET_TYPE(object) == (type))

    struct ArObject {
        AROBJ_HEAD;
    };

    ArObject *Compare(const ArObject *self, const ArObject *other, CompareMode mode);

    ArObject *IteratorGet(ArObject *object, bool reversed);

    ArObject *IteratorNext(ArObject *iterator);

    ArObject *Repr(const ArObject *object);

    ArObject *Str(const ArObject *object);

    ArSize Hash(ArObject *object);

    bool IsNull(const ArObject *object);

    bool IsTrue(const ArObject *object);

    bool BufferGet(ArObject *object, ArBuffer *buffer, BufferFlags flags);

    bool BufferSimpleFill(const ArObject *object, ArBuffer *buffer, BufferFlags flags, unsigned char *raw,
                          ArSize item_size, ArSize nelem, bool writable);

    bool Equal(const ArObject *self, const ArObject *other);

    inline bool EqualStrict(const ArObject *self, const ArObject *other) {
        if (AR_SAME_TYPE(self, other))
            return Equal(self, other);

        return false;
    }

    bool TypeInit(const TypeInfo *type, ArObject *auxiliary);

    void BufferRelease(ArBuffer *buffer);

    template<typename T>
    T *IncRef(T *t) {
        if (t != nullptr && !AR_GET_RC(t).IncStrong())
            return nullptr;

        return t;
    }

    template<typename T>
    T *MakeObject(const TypeInfo *type) {
        auto *ret = (ArObject *) argon::vm::memory::Alloc(type->size);
        if (ret == nullptr)
            return nullptr;

        ret->head_.ref_count_ = memory::RCType::INLINE;
        ret->head_.type_ = type;

        return (T *) ret;
    }

    template<typename T>
    T *MakeObject(TypeInfo *type) {
        auto *ret = MakeObject < T > ((const TypeInfo *) type);
        if (ret != nullptr)
            IncRef(type);

        return ret;
    }

    template<typename T>
    T *MakeGCObject(const TypeInfo *type) {
        // TODO STUB
        auto *ret = MakeObject<T>((const TypeInfo *) type);

        return ret;
    }

    void Release(ArObject *object);

    inline void Release(ArObject **object) {
        Release(*object);
        *object = nullptr;
    }

    template<typename T>
    inline void Release(T *t) {
        Release((ArObject *) t);
    }

    class ARC {
        ArObject *object_ = nullptr;

    public:
        ARC() = default;

        explicit ARC(ArObject *object) : object_(object) {}

        ARC(ARC &other) = delete;

        ~ARC() {
            Release(this->object_);
        }

        ARC &operator=(ArObject *object) {
            Release(this->object_);

            this->object_ = object;

            return *this;
        }

        ARC &operator=(const ARC &other) {
            if (this == &other)
                return *this;

            Release(this->object_);

            this->object_ = IncRef(other.object_);

            return *this;
        }

        ARC &operator=(ARC &&other) = delete;

        ArObject *Get() {
            return this->object_;
        }

        ArObject *Unwrap() {
            auto tmp = this->object_;

            this->object_ = nullptr;

            return tmp;
        }

        explicit operator bool() const {
            return this->object_ != nullptr;
        }
    };

    class RefStore {
        union {
            ArObject *s_value;
            memory::RefCount w_value;
        };

        bool weak_ = false;

    public:
        ~RefStore() {
            this->Release();
        }

        ArObject *Get() {
            if (!this->weak_) {
                if (!this->s_value->head_.ref_count_.IncStrong())
                    return nullptr;

                return this->s_value;
            }

            return (ArObject *) this->w_value.GetObject();
        }

        void Store(ArObject *object, bool weak) {
            if (!weak
                || object->head_.ref_count_.IsStatic()
                || ENUMBITMASK_ISFALSE(AR_GET_TYPE(object)->flags, TypeInfoFlags::WEAKABLE)) {
                object->head_.ref_count_.IncStrong();
                this->s_value = object;
                this->weak_ = false;
                return;
            }

            this->w_value = object->head_.ref_count_.IncWeak();
            this->weak_ = true;
        }

        void Store(ArObject *object) {
            bool strong = true;

            if (this->s_value != nullptr && this->weak_)
                strong = false;

            this->Store(object, strong);
        }

        void Release() {
            if (this->weak_)
                this->w_value.DecWeak();
            else
                this->s_value->head_.ref_count_.DecStrong();

            this->s_value = nullptr;
        }
    };

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_AROBJECT_H_
