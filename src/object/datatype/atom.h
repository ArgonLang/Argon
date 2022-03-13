// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_ATOM_H_
#define ARGON_OBJECT_ATOM_H_

#include "string.h"
#include <object/arobject.h>

namespace argon::object {
    struct Atom : ArObject {
        String *value;
    };

    extern const TypeInfo *type_atom_;

    Atom *AtomNew(const unsigned char *value);

    inline Atom *AtomNew(const char *value) {
        return AtomNew((const unsigned char *) value);
    }

} // namespace argon::object

#endif // !ARGON_OBJECT_ATOM_H_
