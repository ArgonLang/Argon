// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_NIL_H_
#define ARGON_OBJECT_NIL_H_

#include <object/arobject.h>

#define ARGON_OBJECT_NIL (argon::object::IncRef(argon::object::NilVal))

namespace argon::object {
    struct Nil : public ArObject {
    };

    extern const TypeInfo *type_nil_;

    extern Nil *NilVal;

    inline ArObject *ReturnNil(const ArObject *obj) {
        return obj != nullptr ? (ArObject *) obj : NilVal;
    }

} // namespace argon::object

#endif // !ARGON_OBJECT_NIL_H_

