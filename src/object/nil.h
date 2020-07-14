// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_NIL_H_
#define ARGON_OBJECT_NIL_H_

#include "object.h"

namespace argon::object {
    struct Nil : public ArObject {
    };

    extern Nil *NilVal;

    inline Nil *ReturnNil() {
        IncRef(NilVal);
        return NilVal;
    }

} // namespace argon::object

#endif // !ARGON_OBJECT_NIL_H_

