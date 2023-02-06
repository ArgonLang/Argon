// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_AROBJECT_H_
#define ARGON_VM_DATATYPE_AROBJECT_H_

#include <cstddef>

#include <vm/memory/gc.h>

#include "objectdef.h"

namespace argon::vm::datatype {
    extern const TypeInfo *type_type_;

    ArObject *AttributeLoad(const ArObject *object, ArObject *key, bool static_attr);

    ArObject *AttributeLoadMethod(const ArObject *object, ArObject *key, bool *is_method);

    ArObject *ComputeMRO(TypeInfo *type, TypeInfo **bases, unsigned int length);

    ArObject *Compare(const ArObject *self, const ArObject *other, CompareMode mode);

    ArObject *IteratorGet(ArObject *object, bool reversed);

    ArObject *IteratorNext(ArObject *iterator);

    ArObject *Repr(const ArObject *object);

    ArObject *Str(ArObject *object);

    ArObject *TraitNew(const char *name, const char *qname, const char *doc,
                       ArObject *ns, TypeInfo **bases, unsigned int length);

    ArObject *TypeNew(const TypeInfo *type, const char *name, const char *qname, const char *doc,
                      ArObject *ns, TypeInfo **bases, unsigned int length);

    bool AttributeSet(ArObject *object, ArObject *key, ArObject *value, bool static_attr);

    bool BufferGet(ArObject *object, ArBuffer *buffer, BufferFlags flags);

    bool BufferSimpleFill(const ArObject *object, ArBuffer *buffer, BufferFlags flags, unsigned char *raw,
                          ArSize item_size, ArSize nelem, bool writable);

    bool Equal(const ArObject *self, const ArObject *other);

    inline bool EqualStrict(const ArObject *self, const ArObject *other) {
        if (AR_SAME_TYPE(self, other))
            return Equal(self, other);

        return false;
    }

    bool Hash(ArObject *object, ArSize *out_hash);

    inline bool IsBufferable(const ArObject *object) {
        return AR_GET_TYPE(object)->buffer != nullptr && AR_GET_TYPE(object)->buffer->get_buffer != nullptr;
    }

    bool IsNull(const ArObject *object);

    bool IsTrue(const ArObject *object);

    bool TypeInit(TypeInfo *type, ArObject *auxiliary);

    bool TraitIsImplemented(const ArObject *object, const TypeInfo *type);

    int RecursionTrack(ArObject *object);

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

        AR_GET_RC(ret) = memory::RCType::INLINE;
        AR_GET_TYPE(ret) = type;

        return (T *) ret;
    }

    template<typename T>
    T *MakeObject(TypeInfo *type) {
        auto *ret = MakeObject<T>((const TypeInfo *) type);
        if (ret != nullptr)
            IncRef(type);

        return ret;
    }

    template<typename T>
    T *MakeGCObject(const TypeInfo *type, bool track) {
        // N.B: It's risky to track an object before initializing its ReferenceCounter,
        // but we can do it because the GC doesn't cycle until all worker threads have stopped,
        // including the one currently allocating a new object.
        auto *ret = memory::GCNew(type->size, track);

        AR_GET_RC(ret) = memory::RCType::GC;
        AR_GET_TYPE(ret) = type;

        return (T *) ret;
    }

    template<typename T>
    T *MakeGCObject(TypeInfo *type, bool track) {
        auto *ret = MakeGCObject<T>((const TypeInfo *) type, track);
        if (ret != nullptr)
            IncRef(type);

        return ret;
    }

    void BufferRelease(ArBuffer *buffer);

    void Release(ArObject *object);

    template<typename T>
    inline void Release(T **object) {
        Release(*object);
        *object = nullptr;
    }

    template<typename T>
    inline void Release(T *t) {
        Release((ArObject *) t);
    }

    inline void Replace(ArObject **variable, ArObject *value) {
        Release(*variable);
        *variable = value;
    }

    void RecursionUntrack(ArObject *object);

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

        ArObject *GetRawReference() {
            return this->weak_ ? nullptr : this->s_value;
        }

        void Store(ArObject *object, bool strong) {
            if (strong
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
