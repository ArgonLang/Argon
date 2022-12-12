// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_ATOM_H_
#define ARGON_VM_DATATYPE_ATOM_H_

#include "arobject.h"
#include "arstring.h"

namespace argon::vm::datatype {
    struct Atom {
        AROBJ_HEAD;

        String *value;
    };
    extern const TypeInfo *type_atom_;

    /**
     * @brief Create a new Atom object.
     * @param value Value to associate with the atom.
     * @return A pointer to an Atom object, otherwise nullptr.
     */
    Atom *AtomNew(const char *value);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_ATOM_H_
