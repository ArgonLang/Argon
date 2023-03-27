// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_IMPORTER_ISPEC_H_
#define ARGON_VM_IMPORTER_ISPEC_H_

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/function.h>
#include <argon/vm/datatype/module.h>

namespace argon::vm::importer {
    struct ImportSpec {
        AROBJ_HEAD;

        datatype::String *name;
        datatype::String *path;
        datatype::String *origin;

        datatype::Function *loader;

        const datatype::ModuleInit *init;
    };

    extern const datatype::TypeInfo *type_import_spec_;

    ImportSpec *ISpecNew(datatype::String *name, datatype::String *path,
                         datatype::String *origin, datatype::Function *loader);

} // namespace argon::vm::importer

#endif // !ARGON_VM_IMPORTER_ISPEC_H_
