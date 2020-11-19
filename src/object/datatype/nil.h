// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_NIL_H_
#define ARGON_OBJECT_NIL_H_

#include <object/objmgmt.h>

namespace argon::object {
    struct Nil : public ArObject {
    };

    extern const TypeInfo type_nil_;

    extern Nil *NilVal;

    inline Nil *ReturnNil() {
        IncRef(NilVal);
        return NilVal;
    }

    inline ArObject *ReturnNil(const ArObject *obj) {
        if (obj != nullptr)
            return (ArObject *) obj;
        IncRef(NilVal);
        return NilVal;
    }

} // namespace argon::object

#endif // !ARGON_OBJECT_NIL_H_

