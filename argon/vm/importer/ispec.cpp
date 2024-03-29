// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/boolean.h>

#include <argon/vm/importer/ispec.h>

using namespace argon::vm::datatype;
using namespace argon::vm::importer;

const MemberDef ispec_members[] = {
        ARGON_MEMBER("name", MemberType::OBJECT, offsetof(ImportSpec, name), true),
        ARGON_MEMBER("path", MemberType::OBJECT, offsetof(ImportSpec, path), true),
        ARGON_MEMBER("origin", MemberType::OBJECT, offsetof(ImportSpec, origin), true),
        ARGON_MEMBER("loader", MemberType::OBJECT, offsetof(ImportSpec, loader), true),
        ARGON_MEMBER_SENTINEL
};

const ObjectSlots ispec_objslot = {
        nullptr,
        ispec_members,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *import_spec_compare(ImportSpec *self, ArObject *other, CompareMode mode) {
    const auto *o = (ImportSpec *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if ((ArObject *) self == other)
        return BoolToArBool(true);

    return BoolToArBool(Equal((ArObject *) self->name, (ArObject *) o->name) &&
                        Equal((ArObject *) self->path, (ArObject *) o->path) &&
                        Equal((ArObject *) self->origin, (ArObject *) o->origin) &&
                        Equal((ArObject *) self->loader, (ArObject *) o->loader));
}

ArObject *import_spec_repr(const ImportSpec *self) {
    String *loader;
    String *ret;

    if ((loader = (String *) Repr((ArObject *) self->loader)) == nullptr)
        return nullptr;

    ret = StringFormat("<%s -- path: %s, origin: %s, loader: %s>",
                       ARGON_RAW_STRING(self->name),
                       self->path == nullptr ? (unsigned char *) "" : ARGON_RAW_STRING(self->path),
                       self->origin == nullptr ? (unsigned char *) "" : ARGON_RAW_STRING(self->origin),
                       ARGON_RAW_STRING(loader));

    Release(loader);

    return (ArObject *) ret;
}

bool import_spec_dtor(ImportSpec *self) {
    Release(self->name);
    Release(self->path);
    Release(self->origin);
    Release(self->loader);

    return true;
}

TypeInfo ImportSpecType = {
        AROBJ_HEAD_INIT_TYPE,
        "ImportSpec",
        nullptr,
        nullptr,
        sizeof(ImportSpec),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) import_spec_dtor,
        nullptr,
        nullptr,
        nullptr,
        (CompareOp) import_spec_compare,
        (UnaryConstOp) import_spec_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &ispec_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::importer::type_import_spec_ = &ImportSpecType;

ImportSpec *argon::vm::importer::ISpecNew(String *name, String *path, String *origin, Function *loader) {
    auto *ispec = MakeObject<ImportSpec>(type_import_spec_);

    if (ispec != nullptr) {
        ispec->name = IncRef(name);
        ispec->path = IncRef(path);
        ispec->origin = IncRef(origin);
        ispec->loader = IncRef(loader);
        ispec->init = nullptr;
    }

    return ispec;
}
