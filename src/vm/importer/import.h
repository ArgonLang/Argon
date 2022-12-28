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

namespace argon::vm {
    /*
     * Why this?
     *
     * Context contains the current importer, importer in turn uses a call to Eval to initialize the
     * module just imported, to call Eval you need a context that can be retrieved from the current fiber
     * if you are in an Argon thread! But what if you are asked to import a module in a non-Argon thread?
     * In this case Fiber is nullptr and as a result it is not possible to get the current context.
     * Adding a cyclic reference is ugly, but it solves the problem!
     */
    struct Context;
}

namespace argon::vm::importer {
    using ImportModuleCacheEntry = datatype::HEntry<datatype::String, datatype::ArObject *>;

    using ImportModuleCache = datatype::HashMap<datatype::String, datatype::ArObject *>;

    constexpr const char *kExtension[] = {".ar"};

    struct Import {
        AROBJ_HEAD;

        std::mutex lock;

        ImportModuleCache module_cache;

        Context *context;

        datatype::List *loaders;

        datatype::List *locators;

        datatype::List *paths;

        datatype::String *path_sep;
    };

    extern const datatype::TypeInfo *type_import_;

    bool ImportAddPath(Import *imp, const char *path);

    bool ImportAddPath(Import *imp, datatype::String *path);

    Import *ImportNew(Context *context);

    datatype::Module *LoadModule(Import *imp, const char *name, ImportSpec *hint);

    datatype::Module *LoadModule(Import *imp, datatype::String *name, ImportSpec *hint);

} // namespace argon::vm::importer

#endif // !ARGON_VM_IMPORTER_IMPORT_H_
