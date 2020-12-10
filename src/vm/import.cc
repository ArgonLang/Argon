// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <fstream>

#include <lang/compiler.h>

#include <object/arobject.h>
#include <object/datatype/error.h>

#include <modules/builtins.h>
#include <modules/io/iomodule.h>

#include "runtime.h"
#include "areval.h"

#include "import.h"

using namespace argon::memory;
using namespace argon::object;
using namespace argon::vm;

String *mod_sep = nullptr;
String *path_sep = nullptr;

String *exts[] = {nullptr, nullptr, nullptr};    // ".ar", ".arc", ".so"/".dll"

// *** ImportSpec

void impspec_cleanup(ImportSpec *self) {
    Release(self->name);
    Release(self->path);
}

const argon::object::TypeInfo type_import_spec_ = {
        TYPEINFO_STATIC_INIT,
        (const unsigned char *) "import_spec",
        sizeof(ImportSpec),
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
        nullptr,
        nullptr,
        (VoidUnaryOp) impspec_cleanup
};

ImportSpec *argon::vm::ImportSpecNew(String *name, String *path, String *origin, LoaderPtr loader) {
    auto impspec = ArObjectNew<ImportSpec>(RCType::INLINE, &type_import_spec_);

    if (impspec != nullptr) {
        IncRef(name);
        impspec->name = name;
        IncRef(path);
        impspec->path = path;
        IncRef(origin);
        impspec->origin = origin;

        impspec->loader = loader;
        impspec->initfn = nullptr;
    }

    return impspec;
}

// *** Loaders

Module *BuiltinsLoader(Import *import, ImportSpec *spec) {
    return spec->initfn();
}

Module *SourceLoader(Import *import, ImportSpec *spec) {
    argon::object::Code *code = nullptr;
    argon::lang::Compiler compiler;
    std::filebuf infile;

    if (infile.open((char *) spec->origin->buffer, std::ios::in)) {
        std::istream is(&infile);
        code = compiler.Compile(&is);
        infile.close();
    }

    if (code != nullptr) {
        auto module = ModuleNew((const char *) spec->name->buffer, "");
        Frame *frame = FrameNew(code, module->module_ns, nullptr);

        if (GetRoutine() != nullptr) {
            Release(Eval(GetRoutine(), frame));
            FrameDel(frame);
        } else {
            auto routine = RoutineNew(frame);
            SetRoutineMain(routine);
            Release(Eval(routine, frame));
            SetRoutineMain(nullptr);

            if (routine->panic != nullptr)
                Panic(routine->panic->object);

            RoutineDel(routine);
        }

        if (IsPanicking()) {
            Release(module);
            module = nullptr;
        }

        return module;
    }

    return nullptr;
}

// *** Locators

struct Builtins {
    const char *name;

    InitBuiltins init;
};

ImportSpec *BuiltinsLocator(Import *import, String *name, String *package) {
    static Builtins builtins[] = {{"builtins", argon::modules::BuiltinsNew},
                                  {"io",       argon::modules::io::IONew}};
    ImportSpec *imp;

    for (auto &builtin : builtins) {
        if (StringEq(name, (unsigned char *) builtin.name, strlen(builtin.name))) {
            if ((imp = ImportSpecNew(name, nullptr, nullptr, BuiltinsLoader)) == nullptr)
                return nullptr;

            imp->initfn = builtin.init;
            return imp;
        }
    }

    return nullptr;
}

ImportSpec *CheckSource(String *base, String *mod_path, String *name) {
    ImportSpec *imp = nullptr;
    String *file;
    String *path;
    bool ok;

    if ((path = StringConcat(base, mod_path)) == nullptr)
        return nullptr;

    // TODO: impl all extensions
    if ((file = StringConcat(path, exts[0])) == nullptr) {
        Release(path);
        return nullptr;
    }

    std::ifstream infile((char *) file->buffer);
    ok = infile.good();
    infile.close();

    if (ok) {
        // Extract origin package
        arsize last_sep = StringRFind(file, path_sep);
        String *pkg = last_sep > -1 ? StringSubs(file, 0, last_sep + 1) : StringIntern("");
        if (pkg != nullptr) {
            imp = ImportSpecNew(name, pkg, file, SourceLoader);
            Release(pkg);
        }
    }

    Release(file);
    Release(path);
    return imp;
}

ImportSpec *SourceLocator(Import *import, String *name, String *package) {
    String *mod_name;
    String *mod_path;

    ImportSpec *imp = nullptr;

    // Convert import expression to OS path e.g. x::y::z -> x/y/z
    if ((mod_path = StringReplaceAll(name, mod_sep, path_sep)) == nullptr)
        return nullptr;

    // Extract module name
    arsize last_sep = StringRFind(name, mod_sep);
    if (last_sep > 0) {
        if ((mod_name = StringSubs(name, last_sep + mod_sep->len, 0)) == nullptr) {
            Release(mod_path);
            return nullptr;
        }
    } else {
        IncRef(name);
        mod_name = name;
    }

    if (package != nullptr)
        imp = CheckSource(package, mod_path, mod_name);

    if (imp == nullptr) {
        // TODO: use iterator!
        for (size_t i = 0; i < import->paths->len && imp == nullptr; i++)
            imp = CheckSource((String *) import->paths->objects[i], mod_path, mod_name);
    }

    Release(mod_name);
    Release(mod_path);
    return imp;
}

static LocatorPtr locators[] = {BuiltinsLocator, SourceLocator};

// *** Locators EOL

Import *argon::vm::ImportNew() {
#define INIT_CONST_STR(name, string)                                    \
    if (name==nullptr && ((name = StringIntern(string)) == nullptr))    \
        goto error

    auto importer = (Import *) Alloc(sizeof(Import));

    if (importer != nullptr) {
        if ((importer->modules = MapNew()) == nullptr)
            goto error;

        if ((importer->paths = ListNew()) == nullptr)
            goto error;

        //importer->entries = nullptr;

        // Init constants

        INIT_CONST_STR(mod_sep, "::");

#ifdef __WIN32
            INIT_CONST_STR(path_sep, "\\");
            INIT_CONST_STR(exts[2], ".dll");
#else
        INIT_CONST_STR(path_sep, "/");
        INIT_CONST_STR(exts[2], ".so");
#endif
        INIT_CONST_STR(exts[0], ".ar");
        INIT_CONST_STR(exts[1], ".arc");

        // EOL

        return importer;
    }

    error:
    ImportDel(importer);
    return nullptr;
#undef INIT_CONST_STR
}

void argon::vm::ImportDel(Import *import) {
    if (import == nullptr)
        return;

    Release(import->modules);
    Release(import->paths);

    Free(import);
}

bool argon::vm::ImportAddPath(Import *import, const char *path) {
    String *str = StringNew(path);
    bool ok = false;

    if (str != nullptr) {
        ok = ImportAddPath(import, str);
        Release(str);
    }

    return ok;
}

Module *argon::vm::ImportModule(Import *import, String *name, String *package) {
    Module *mod;

    // Check cache
    if ((mod = (Module *) MapGet(import->modules, name)) != nullptr)
        return mod;

    // Call Locators
    ImportSpec *spec = nullptr;
    for (auto &locator : locators) {
        if ((spec = locator(import, name, package)) != nullptr)
            break;

        if (IsPanicking())
            break;
    }

    if (spec == nullptr) {
        ErrorFormat(&error_module_notfound, "No module named '%s'", name->buffer);
        return nullptr;
    }

    // Call loader
    mod = spec->loader(import, spec);

    if (mod != nullptr) {
        String *s_imps;
        bool ok;

        // Set ImportSpec
        s_imps = StringIntern("__imps");
        ok = ModuleAddProperty(mod, s_imps, spec, PropertyInfo(PropertyType::PUBLIC | PropertyType::CONST));
        Release(s_imps);

        // Fill cache, IF FAIL, delete the newly loaded module
        if (!ok || !MapInsert(import->modules, name, mod)) {
            Release(mod);
            mod = nullptr;
        }
    }

    return mod;
}

Module *argon::vm::ImportModule(Import *import, const char *name, const char *package) {
    String *apkg = nullptr;
    String *aname;

    if ((aname = StringNew(name)) == nullptr)
        return nullptr;

    if (package != nullptr) {
        if ((apkg = StringNew(package)) == nullptr) {
            Release(aname);
            return nullptr;
        }
    }

    auto module = ImportModule(import, aname, apkg);
    Release(aname);
    Release(apkg);

    return module;
}