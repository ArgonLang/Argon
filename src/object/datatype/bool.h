// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_BOOL_H_
#define ARGON_OBJECT_BOOL_H_

#include <object/arobject.h>

namespace argon::object {
    struct Bool : public ArObject {
        bool value;
    };

    extern const TypeInfo type_bool_;

    extern Bool *True;
    extern Bool *False;

    inline ArObject *BoolToArBool(bool value) {
        return value ? True : False; // IncRef is useless, True & False are static!
    }

} // namespace argon::object

#endif // !ARGON_OBJECT_BOOL_H_

