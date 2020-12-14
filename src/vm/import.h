// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_IMPORT_H_
#define ARGON_VM_IMPORT_H_

#include <object/arobject.h>
#include <object/datatype/list.h>
#include <object/datatype/map.h>
#include <object/datatype/module.h>

namespace argon::vm {

    using LoaderPtr = argon::object::Module *(*)(struct Import *import, struct ImportSpec *spec);

    using InitBuiltins = argon::object::Module *(*)();

    struct ImportSpec : public argon::object::ArObject {
        argon::object::String *name;
        argon::object::String *path;
        argon::object::String *origin;

        LoaderPtr loader;
        InitBuiltins initfn;
    };

    ImportSpec *ImportSpecNew(argon::object::String *name, argon::object::String *path, argon::object::String *origin,
                              LoaderPtr loader);

    // *** ImportSpec EOL

    using LocatorPtr = ImportSpec *(*)(struct Import *import, argon::object::String *name,
                                       argon::object::String *package_path);

    struct Import {
        argon::object::Map *modules;

        argon::object::List *paths;
    };

    Import *ImportNew();

    void ImportDel(Import *import);

    bool ImportAddPath(Import *import, const char *path);

    inline bool ImportAddPath(Import *import, argon::object::String *path) {
        return argon::object::ListAppend(import->paths, path);
    }

    inline bool ImportAddPaths(Import *import, argon::object::List *list) {
        return argon::object::ListConcat(import->paths, list);
    }

    argon::object::Module *ImportAddModule(Import *import, const char *name);

    argon::object::Module *ImportAddModule(Import *import, argon::object::String *name);

    argon::object::Module *ImportModule(Import *import, argon::object::String *name, argon::object::String *package);

    argon::object::Module *ImportModule(Import *import, const char *name, const char *package);

} // namespace argon::vm::import;


#endif // !ARGON_VM_IMPORT_H_
