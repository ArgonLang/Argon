// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_TUPLE_H_
#define ARGON_OBJECT_TUPLE_H_

#include "object.h"

namespace argon::object {
    struct Tuple : ArObject {
        ArObject **objects;
        size_t len;
    };

    Tuple *TupleNew(const ArObject *sequence);

} // namespace argon::object

#endif // !ARGON_OBJECT_TUPLE_H_
