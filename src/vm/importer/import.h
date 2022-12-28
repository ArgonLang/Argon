// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_IMPORTER_IMPORT_H_
#define ARGON_VM_IMPORTER_IMPORT_H_

#include <mutex>

#include <vm/datatype/arobject.h>
#include <vm/datatype/arstring.h>
#include <vm/datatype/code.h>
#include <vm/datatype/list.h>
#include <vm/datatype/hashmap.h>
#include <vm/datatype/module.h>

#include "ispec.h"

namespace argon::vm::importer {
    using ImportModuleCacheEntry = datatype::HEntry<datatype::String, datatype::ArObject *>;

    using ImportModuleCache = datatype::HashMap<datatype::String, datatype::ArObject *>;

    constexpr const char *kExtension[] = {".ar"};

    struct Import {
        AROBJ_HEAD;

        std::mutex lock;

        ImportModuleCache module_cache;

        datatype::List *loaders;

        datatype::List *locators;

        datatype::List *paths;

        datatype::String *path_sep;
    };

    extern const datatype::TypeInfo *type_import_;

    bool ImportAddPath(Import *imp, const char *path);

    bool ImportAddPath(Import *imp, datatype::String *path);

    Import *ImportNew();

    datatype::Module *ImportLoadCode(Import *imp, datatype::String *name, ImportSpec *hint);

} // namespace argon::vm::importer

#endif // !ARGON_VM_IMPORTER_IMPORT_H_
