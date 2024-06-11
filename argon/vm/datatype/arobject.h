// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_AROBJECT_H_
#define ARGON_VM_DATATYPE_AROBJECT_H_

#include <cstddef>

#include <argon/util/macros.h>

#include <argon/vm/memory/gc.h>

#include <argon/vm/datatype/objectdef.h>

namespace argon::vm::datatype {
    _ARGONAPI extern const TypeInfo *type_type_;

    ArObject *AttributeLoad(const ArObject *object, ArObject *key, bool static_attr);

    ArObject *AttributeLoadMethod(const ArObject *object, ArObject *key, bool *is_method);

    ArObject *AttributeLoadMethod(const ArObject *object, const char *key);

    ArObject *ComputeMRO(TypeInfo *type, TypeInfo **bases, unsigned int length);

    ArObject *Compare(const ArObject *self, const ArObject *other, CompareMode mode);

    ArObject *ExecBinaryOp(ArObject *left, ArObject *right, int offset);

    ArObject *ExecBinaryOpOriented(ArObject *left, ArObject *right, int offset);

    ArObject *IteratorGet(ArObject *object, bool reversed);

    ArObject *IteratorNext(ArObject *iterator);

    ArObject *Repr(ArObject *object);

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

    bool TraitIsImplemented(const TypeInfo *obj_type, const TypeInfo *type);

    bool TypeOF(const ArObject *object, const TypeInfo *type);

    int MonitorAcquire(ArObject *object);

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

        AR_UNSAFE_GET_RC(ret) = (ArSize) memory::RCType::INLINE;
        AR_GET_TYPE(ret) = type;
        AR_UNSAFE_GET_MON(ret) = nullptr;

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
    T *MakeGCObject(const TypeInfo *type) {
        return (T *) memory::GCNew(type, false);
    }

    template<typename T>
    T *MakeGCObject(TypeInfo *type) {
        auto *ret = (T *) memory::GCNew(type, false);
        if (ret != nullptr)
            IncRef(type);

        return ret;
    }

    void BufferRelease(ArBuffer *buffer);

    void MonitorDestroy(ArObject *object);

    void MonitorRelease(ArObject *object);

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

        template<typename T>
        ARC &operator=(T *object) {
            Release(this->object_);

            this->object_ = (ArObject *) object;

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

        void Discard() {
            Release(this->object_);
            this->object_ = nullptr;
        }
    };

    class RefStore {
        union {
            ArObject *s_value;
            memory::RefCount w_value;
        };

        bool weak_ = false;

    public:
        ~RefStore();

        ArObject *Get();

        [[nodiscard]] ArObject *GetRawReference() const;

        void Store(ArObject *object, bool strong);

        void Store(ArObject *object);

        void Release();
    };

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_AROBJECT_H_
