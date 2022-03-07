// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_TUPLE_H_
#define ARGON_OBJECT_TUPLE_H_

#include <object/arobject.h>

#define ARGON_OBJECT_TUPLE_ERROR(err)   argon::object::TupleReturn(nullptr, (err))
#define ARGON_OBJECT_TUPLE_SUCCESS(obj) argon::object::TupleReturn((obj), nullptr)

namespace argon::object {
    struct Tuple : ArObject {
        ArObject **objects;
        ArSize len;
    };

    extern const TypeInfo *type_tuple_;

    Tuple *TupleNew(ArSize len);

    Tuple *TupleNew(const ArObject *sequence);

    Tuple *TupleNew(ArObject *result, ArObject *error);

    inline Tuple *TupleReturn(ArObject *result, ArObject *error) {
        Tuple *ret;

        if(result == nullptr && error == nullptr)
            return nullptr;

        ret = TupleNew(result, error);
        Release(result);
        Release(error);
        return ret;
    }

    Tuple *TupleNew(ArObject **objects, ArSize count);

    Tuple *TupleNew(const char *fmt, ...);

    ArObject *TupleGetItem(Tuple *self, ArSSize i);

    bool TupleInsertAt(Tuple *tuple, ArSize idx, ArObject *obj);

    bool TupleUnpack(Tuple *tuple, const char *fmt, ...);

} // namespace argon::object

#endif // !ARGON_OBJECT_TUPLE_H_
