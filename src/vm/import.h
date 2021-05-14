// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_IMPORT_H_
#define ARGON_VM_IMPORT_H_

#include <object/arobject.h>
#include <object/datatype/function.h>
#include <object/datatype/list.h>
#include <object/datatype/map.h>
#include <object/datatype/module.h>
#include <object/datatype/tuple.h>

namespace argon::vm {
    using InitBuiltins = argon::object::Module *(*)();

    struct ImportSpec : public argon::object::ArObject {
        argon::object::String *name;
        argon::object::String *path;
        argon::object::String *origin;

        argon::object::Function *loader;

        InitBuiltins initfn;
    };

    extern const argon::object::TypeInfo type_import_spec_;

    ImportSpec *ImportSpecNew(argon::object::String *name, argon::object::String *path, argon::object::String *origin,
                              argon::object::Function *loader);

    struct Import : argon::object::ArObject {
        argon::object::Map *modules;
        argon::object::List *paths;
        argon::object::Tuple *extensions; // ".ar", ".arc", ".dll"/".dylib"/".so"

        argon::object::List *locators;
        argon::object::List *loaders;

        argon::object::String *path_sep;
    };

    extern const argon::object::TypeInfo type_import_;

    Import *ImportNew();

    bool ImportAddPath(Import *import, const char *path);

    bool ImportAddPath(Import *import, argon::object::String *path);

    bool ImportAddPaths(Import *import, argon::object::List *paths);

    argon::object::Module *ImportAddModule(Import *import, const char *name);

    argon::object::Module *ImportModule(Import *import, const char *name);

    argon::object::Module *ImportModule(Import *import, argon::object::String *name, argon::object::String *package);

    inline argon::object::Module *ImportModule(Import *import, argon::object::String *name) {
        return ImportModule(import, name, nullptr);
    }

} // namespace argon::vm::import;


#endif // !ARGON_VM_IMPORT_H_
