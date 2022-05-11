// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "bool.h"
#include "map.h"
#include "string.h"

#include "atom.h"

using namespace argon::object;

static Map *gat = nullptr; // Global Atoms Table

ARGON_FUNCTION5(atom_, new, "", 1, false) {
    const auto *str = (String *) argv[0];

    if (!CheckArgs("s:value", func, argv, count))
        return nullptr;

    return AtomNew(str->buffer);
}

const NativeFunc atom_method[] = {
        atom_new_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots atom_obj = {
        atom_method,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

void atom_cleanup(const Atom *self) {
    Release(self->value);
}

ArObject *atom_compare(const Atom *self, const ArObject *other, CompareMode mode) {
    if (mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(self == other);
}

bool atom_is_true(const Atom *self) {
    return true;
}

ArSize atom_hash(const Atom *self) {
    return AR_GET_TYPE(self->value)->hash(self->value);
}

ArObject *atom_str(const Atom *self) {
    return StringNewFormat("@%s", self->value->buffer);
}

const TypeInfo AtomType = {
        TYPEINFO_STATIC_INIT,
        "atom",
        nullptr,
        sizeof(Atom),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) atom_cleanup,
        nullptr,
        (CompareOp) atom_compare,
        (BoolUnaryOp) atom_is_true,
        (SizeTUnaryOp) atom_hash,
        nullptr,
        (UnaryOp) atom_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &atom_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::type_atom_ = &AtomType;

Atom *argon::object::AtomNew(const unsigned char *value) {
    ArSize vlen = strlen((const char *) value);
    Atom *atom;

    if (gat == nullptr && (gat = MapNew()) == nullptr)
        return nullptr;

    if ((atom = (Atom *) MapGetFrmStr(gat, (const char *) value, vlen)) != nullptr)
        return atom;

    if ((atom = ArObjectNew<Atom>(RCType::INLINE, type_atom_)) == nullptr)
        return nullptr;

    if ((atom->value = StringNew((const char *) value)) == nullptr) {
        Release(atom);
        return nullptr;
    }

    if (!MapInsert(gat, atom->value, atom)) {
        Release(atom);
        return nullptr;
    }

    return atom;
}