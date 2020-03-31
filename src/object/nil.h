// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_NIL_H_
#define ARGON_OBJECT_NIL_H_

#include <memory/memory.h>

#include "object.h"

namespace argon::object {
    class Nil : public Object {
        Nil();

        bool EqualTo(const Object *other) override;

        size_t Hash() override;

    public:
        static Nil *NilValue() {
            static Nil nil;
            IncStrongRef(&nil);
            return &nil;
        }
    };

    inline const TypeInfo type_nil_ = {
            .name=(const unsigned char *) "nil",
            .size=sizeof(Nil)
    };
} // namespace argon::object

#endif // !ARGON_OBJECT_NIL_H_

