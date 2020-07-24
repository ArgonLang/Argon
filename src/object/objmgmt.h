// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_OBJMGMT_H_
#define ARGON_OBJECT_OBJMGMT_H_

#include <memory/memory.h>

#include "arobject.h"
#include "gc.h"

namespace argon::object {

    template<typename T>
    inline typename std::enable_if<std::is_base_of<ArObject, T>::value, T>::type *
    ArObjectNew(RCType init, const TypeInfo *type) {
        auto obj = (T *) memory::Alloc(sizeof(T));

        if (obj != nullptr) {
            obj->ref_count = RefBits((unsigned char) init);
            obj->type = type;
        }

        return obj;
    }

    template<typename T>
    inline typename std::enable_if<std::is_base_of<ArObject, T>::value, T>::type *
    ArObjectNewGC(const TypeInfo *type) {
        auto obj = (T *) GCNew(sizeof(T));

        if (obj != nullptr) {
            obj->ref_count = RefBits((unsigned char) RCType::GC);
            obj->type = type;
        }

        return obj;
    }

    inline bool IsNumber(const ArObject *obj) { return obj->type->number_actions != nullptr; }

    inline bool IsSequence(const ArObject *obj) { return obj->type->sequence_actions != nullptr; }

    inline bool AsIndex(const ArObject *obj) {
        return obj->type->number_actions != nullptr && obj->type->number_actions->as_index;
    }

    inline bool IsMap(const ArObject *obj) { return obj->type->map_actions != nullptr; }

    inline bool IsTrue(const ArObject *obj) {
        if (IsSequence(obj) && obj->type->sequence_actions->length != nullptr)
            return obj->type->sequence_actions->length((ArObject *) obj) > 0;
        else if (IsMap(obj) && obj->type->map_actions->length != nullptr)
            return obj->type->map_actions->length((ArObject *) obj) > 0;
        if (obj->type->is_true != nullptr)
            return obj->type->is_true((ArObject *) obj);
        return false;
    }

    inline void IncRef(ArObject *obj) {
        if (obj != nullptr)
            obj->ref_count.IncStrong();
    };

    inline void Release(ArObject *obj) {
        if (obj == nullptr)
            return;
        if (obj->ref_count.DecStrong()) {
            if (obj->type->cleanup != nullptr)
                obj->type->cleanup(obj);
            argon::memory::Free(obj);
        }
    }

} // namespace argon::object

#endif // !ARGON_OBJECT_OBJMGMT_H_
