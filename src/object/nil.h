// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_NIL_H_
#define ARGON_OBJECT_NIL_H_

#include <new>
#include <memory/memory.h>

#include "object.h"

namespace argon::object {
    class Nil : public Object {
        Nil() { IncStrongRef(this); };

    public:
        static Nil *NilValue() {
            static Nil nil;
            IncStrongRef(&nil);
            return &nil;
        }
    };
} // namespace argon::object

#endif // !ARGON_OBJECT_NIL_H_

