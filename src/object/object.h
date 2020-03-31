// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_OBJECT_H_
#define ARGON_OBJECT_OBJECT_H_

#include <atomic>

#include <memory/memory.h>

namespace argon::object {
    struct TypeInfo {
        const unsigned char *name;
        unsigned short size;
    };

    class Object {
    public:
        std::atomic_uintptr_t strong_or_ref = 0;
        const TypeInfo *type;

        explicit Object(const TypeInfo *type) : type(type) {}

        virtual ~Object() = default;

        virtual bool EqualTo(const Object *other) = 0;

        virtual size_t Hash() = 0;
    };


    template<typename T, typename ...Args>
    inline T *NewObject(Args ...args) {
        T *obj = argon::memory::AllocObject<T>(args...);
        obj->strong_or_ref++;
        return obj;
    }

    inline void ReleaseObject(argon::object::Object *object) {
        if (object->strong_or_ref.fetch_sub(1) == 1)
            argon::memory::FreeObject<argon::object::Object>(object);
    }

    inline void IncStrongRef(Object *obj) { obj->strong_or_ref++; }

    class ObjectContainer {
        Object *obj_ = nullptr;

    public:
        ObjectContainer() = default;

        explicit ObjectContainer(Object *obj) : obj_(obj) { IncStrongRef(this->obj_); }

        ~ObjectContainer() {
            if (this->obj_ != nullptr)
                ReleaseObject(this->obj_);
        }

        ObjectContainer &operator=(ObjectContainer &&other) noexcept {
            if (this->obj_ != nullptr)
                ReleaseObject(this->obj_);
            this->obj_ = other.obj_;
            other.obj_ = nullptr;
            return *this;
        }

        Object *Get() { return this->obj_; }
    };

    template<typename T, typename ...Args>
    ObjectContainer MakeOwner(Args ...args) { return ObjectContainer(argon::memory::AllocObject<T>(args...)); }

} // namespace argon::object

#endif // !ARGON_OBJECT_OBJECT_H_
