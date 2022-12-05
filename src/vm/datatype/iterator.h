// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_ITERATOR_H_
#define ARGON_VM_DATATYPE_ITERATOR_H_

#include "arobject.h"

namespace argon::vm::datatype {
    template<typename T>
    struct Iterator {
        AROBJ_HEAD;

        T *iterable;

        ArSize index;

        bool reverse;

        Iterator() = delete;
    };

    using IteratorGeneric = Iterator<ArObject>;

    inline ArObject *IteratorIter(ArObject *object, bool reversed) {
        auto *self = (IteratorGeneric *) object;

        if (self->reverse == reversed)
            return (ArObject *) IncRef(self);

        return IteratorGet(self->iterable, reversed);
    }
}

#endif // !ARGON_VM_DATATYPE_ITERATOR_H_
