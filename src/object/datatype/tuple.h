// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_TUPLE_H_
#define ARGON_OBJECT_TUPLE_H_

#include <object/arobject.h>

#define TUPLE_RETURN(result, error)             \
    {                                           \
    ArObject *ret = TupleNew(result, error);    \
    Release(result);                            \
    Release(error);                             \
    return ret;                                 \
    }

namespace argon::object {
    struct Tuple : ArObject {
        ArObject **objects;
        size_t len;
    };

    extern const TypeInfo type_tuple_;

    Tuple *TupleNew(size_t len);

    Tuple *TupleNew(const ArObject *sequence);

    Tuple *TupleNew(ArObject *result, ArObject *error);

    ArObject *TupleGetItem(Tuple *self, ArSSize i);

    bool TupleInsertAt(Tuple *tuple, size_t idx, ArObject *obj);

} // namespace argon::object

#endif // !ARGON_OBJECT_TUPLE_H_
