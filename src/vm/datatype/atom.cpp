// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "boolean.h"
#include "dict.h"

#include "atom.h"

using namespace argon::vm::datatype;

/// Global atoms table
static Dict *gat = nullptr;

ArObject *atom_compare(const ArObject *self, const ArObject *other, CompareMode mode) {
    if (mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(self == other);
}

ArObject *atom_repr(const Atom *self) {
    return (ArObject *) StringFormat("@%s", ARGON_RAW_STRING(self->value));
}

ArSize atom_hash(Atom *self) {
    return AR_GET_TYPE(self->value)->hash((ArObject *) self->value);
}

bool atom_dtor(Atom *self) {
    Release(self->value);
    return true;
}

TypeInfo AtomType = {
        AROBJ_HEAD_INIT_TYPE,
        "Atom",
        nullptr,
        nullptr,
        sizeof(Atom),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) atom_dtor,
        nullptr,
        (ArSize_UnaryOp) atom_hash,
        nullptr,
        atom_compare,
        (UnaryConstOp) atom_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_atom_ = &AtomType;

Atom *argon::vm::datatype::AtomNew(const char *value) {
    Atom *atom;

    if (gat == nullptr) {
        gat = DictNew();
        if (gat == nullptr)
            return nullptr;
    }

    if (value == nullptr || strlen(value) == 0)
        return nullptr;

    if ((atom = (Atom *) DictLookup(gat, value)) != nullptr)
        return atom;

    if ((atom = MakeObject<Atom>(type_atom_)) == nullptr)
        return nullptr;

    if ((atom->value = StringNew(value)) == nullptr) {
        Release(atom);
        return nullptr;
    }

    if (!DictInsert(gat, (ArObject *) atom->value, (ArObject *) atom)) {
        Release(atom);
        return nullptr;
    }

    return atom;
}