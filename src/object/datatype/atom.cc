// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "hash_magic.h"

#include "bool.h"
#include "map.h"
#include "string.h"

#include "atom.h"

using namespace argon::object;

static Map *gat = nullptr; // Global Atoms Table

void atom_cleanup(const Atom *self) {
    argon::memory::Free(self->value);
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
    return self->hash;
}

ArObject *atom_str(const Atom *self) {
    return StringNewFormat("@%s", self->value);
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
        (UnaryOp) atom_str,
        nullptr,
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
const TypeInfo *argon::object::type_atom_ = &AtomType;

Atom *argon::object::AtomNew(const unsigned char *value) {
    ArSize vlen = strlen((const char *) value);
    Atom *atom;
    String *key;

    if (gat == nullptr && (gat = MapNew()) == nullptr)
        return nullptr;

    if ((atom = (Atom *) MapGetFrmStr(gat, (const char *) value, vlen)) != nullptr)
        return atom;

    if ((atom = ArObjectNew<Atom>(RCType::INLINE, type_atom_)) == nullptr)
        return nullptr;

    if ((atom->value = ArObjectNewRaw<unsigned char *>(vlen + 1)) == nullptr) {
        Release(atom);
        return nullptr;
    }

    argon::memory::MemoryCopy(atom->value, value, vlen);
    atom->value[vlen] = '\0';
    atom->hash = HashBytes(atom->value, vlen);

    if ((key = StringNew((const char *) value)) == nullptr) {
        Release(atom);
        return nullptr;
    }

    if (!MapInsert(gat, key, atom)) {
        Release(atom);
        Release(key);
        return nullptr;
    }

    Release(key);
    return atom;
}