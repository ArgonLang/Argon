// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_ITERATOR_H_
#define ARGON_VM_DATATYPE_ITERATOR_H_

#include <mutex>

#include "arobject.h"

namespace argon::vm::datatype {
    template<typename T>
    struct Iterator {
        AROBJ_HEAD;

        std::mutex lock;

        T *iterable;

        ArSize index;

        bool reverse;

        Iterator() = delete;
    };

    template<typename T, typename C>
    struct CursorIterator {
        AROBJ_HEAD;

        std::mutex lock;

        T *iterable;

        C *cursor;

        bool reverse;

        CursorIterator() = delete;
    };

    using IteratorGeneric = Iterator<ArObject>;

    inline ArObject *IteratorIter(ArObject *object, bool reversed) {
        auto *self = (IteratorGeneric *) object;

        if (self->reverse == reversed)
            return (ArObject *) IncRef(self);

        return IteratorGet(self->iterable, reversed);
    }

    inline bool IteratorDtor(IteratorGeneric *iterator){
        Release(iterator->iterable);

        iterator->lock.~mutex();

        return true;
    }

    inline void IteratorTrace(IteratorGeneric *self, Void_UnaryOp trace){
        trace(self->iterable);
    }

    using CursorIteratorGeneric = CursorIterator<ArObject, void>;

    inline ArObject *CursorIteratorIter(ArObject *object, bool reversed) {
        auto *self = (CursorIteratorGeneric *) object;

        if (self->reverse == reversed)
            return (ArObject *) IncRef(self);

        return IteratorGet(self->iterable, reversed);
    }
}

#endif // !ARGON_VM_DATATYPE_ITERATOR_H_
