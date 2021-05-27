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
        ArSize len;
    };

    extern const TypeInfo *type_tuple_;

    Tuple *TupleNew(ArSize len);

    Tuple *TupleNew(const ArObject *sequence);

    Tuple *TupleNew(ArObject *result, ArObject *error);

    ArObject *TupleGetItem(Tuple *self, ArSSize i);

    bool TupleInsertAt(Tuple *tuple, ArSize idx, ArObject *obj);

} // namespace argon::object

#endif // !ARGON_OBJECT_TUPLE_H_
