// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_TUPLE_H_
#define ARGON_OBJECT_TUPLE_H_

#include <object/arobject.h>

namespace argon::object {
    struct Tuple : ArObject {
        ArObject **objects;
        size_t len;
    };

    extern const TypeInfo type_tuple_;

    Tuple *TupleNew(const ArObject *sequence);

    Tuple *TupleNew(size_t len);

    ArObject *TupleGetItem(Tuple *tuple, arsize i);

    bool TupleInsertAt(Tuple *tuple, size_t idx, ArObject *obj);

} // namespace argon::object

#endif // !ARGON_OBJECT_TUPLE_H_
