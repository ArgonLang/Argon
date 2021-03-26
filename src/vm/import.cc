// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <fstream>

#include <lang/compiler.h>

#include <module/builtins.h>
#include <module/io/iomodule.h>
#include <module/math.h>
#include <module/runtime.h>

#include <object/datatype/error.h>
#include <object/datatype/nil.h>

#include "runtime.h"
#include "import.h"
#include "areval.h"

using namespace argon::object;
using namespace argon::vm;

bool import_spec_is_true(ArObject *self) {
    return true;
}

bool import_spec_equal(ImportSpec *self, ArObject *other) {
    auto *o = (ImportSpec *) other;

    if (self == other)
        return true;

    return AR_SAME_TYPE(self, other)
           && AR_EQUAL(self->name, o->name)
           && AR_EQUAL(self->path, o->path)
           && AR_EQUAL(self->origin, o->origin)
           && AR_EQUAL(self->loader, o->loader);
}

ArObject *import_spec_str(ImportSpec *self) {
    Tuple *args = TupleNew(4);
    String *ret;

    if (args == nullptr)
        return nullptr;

    TupleInsertAt(args, 0, self->name);
    TupleInsertAt(args, 1, self->path);
    TupleInsertAt(args, 2, self->origin);
    TupleInsertAt(args, 3, self->loader);

    ret = StringCFormat("ImportSpec(name: %s, path: %s, origin: %s, loader: %s)", args);
    Release(args);

    return ret;
}

void import_spec_cleanup(ImportSpec *self) {
    Release(self->name);
    Release(self->path);
    Release(self->origin);

    Release(self->loader);
}

const argon::object::TypeInfo argon::vm::type_import_spec_ = {
        TYPEINFO_STATIC_INIT,
        "import_spec",
        nullptr,
        sizeof(ImportSpec),
        nullptr,
        (VoidUnaryOp) import_spec_cleanup,
        nullptr,
        nullptr,
        (BoolBinOp) import_spec_equal,
        import_spec_is_true,
        nullptr,
        (UnaryOp) import_spec_str,
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

ImportSpec *argon::vm::ImportSpecNew(String *name, String *path, String *origin, Function *loader) {
    auto spec = ArObjectNew<ImportSpec>(RCType::INLINE, &type_import_spec_);

    if (spec != nullptr) {
        spec->name = IncRef(name);
        spec->path = IncRef(path);
        spec->origin = IncRef(origin);
        spec->loader = IncRef(loader);
        spec->initfn = nullptr;
    }

    return spec;
}

bool import_is_true(ArObject *self) {
    return true;
}

bool import_equal(Import *import, ArObject *other) {
    return import == other;
}

ArObject *import_str(Import *self) {
    Tuple *args = TupleNew(2);
    String *ret;

    if (args == nullptr)
        return nullptr;

    TupleInsertAt(args, 0, self->modules);
    TupleInsertAt(args, 1, self->paths);
    TupleInsertAt(args, 2, self->locators);
    TupleInsertAt(args, 3, self->loaders);

    ret = StringCFormat("Import(modules: %s, paths: %s, locators: %s, loaders: %s)", args);

    Release(args);
    return ret;
}

void import_cleanup(Import *self) {
    Release(self->modules);
    Release(self->paths);
    Release(self->extensions);
    Release(self->locators);
    Release(self->loaders);

    Release(self->path_sep);
}

void import_trace(Import *self, VoidUnaryOp trace) {
    trace(self->modules);
    trace(self->paths);
    trace(self->locators);
    trace(self->loaders);
}

const argon::object::TypeInfo argon::vm::type_import_ = {
        TYPEINFO_STATIC_INIT,
        "import",
        nullptr,
        sizeof(Import),
        nullptr,
        (VoidUnaryOp) import_cleanup,
        (Trace) import_trace,
        nullptr,
        (BoolBinOp) import_equal,
        import_is_true,
        nullptr,
        (UnaryOp) import_str,
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

// LOADERS

ARGON_FUNCTION5(import_, builtins_loader,
                "Load built-in modules."
                ""
                "- Parameters:"
                "   - import: import instance."
                "   - spec: ImportSpec instance describing what to load."
                "- Returns: new module.", 2, false) {
    auto *import = (Import *) argv[0];
    auto *spec = (ImportSpec *) argv[1];
    Module *module;

    if (!AR_TYPEOF(import, type_import_))
        return ErrorFormat(&error_type_error, "expected Import as first arg, found '%s'", AR_TYPE_NAME(import));

    if (!AR_TYPEOF(spec, type_import_spec_))
        return ErrorFormat(&error_type_error, "expected ImportSpec as second arg, found '%s'", AR_TYPE_NAME(spec));

    module = spec->initfn();

    if (!ModuleAddProperty(module, "__spec", spec, MODULE_ATTRIBUTE_PUB_CONST)) {
        Release(module);
        return nullptr;
    }

    if (!MapInsert(import->modules, spec->name, module))
        Release((ArObject **) &module);

    return module;
}

ARGON_FUNCTION5(import_, source_loader,
                "Load external modules from sources."
                ""
                "- Parameters:"
                "   - import: import instance."
                "   - spec: ImportSpec instance describing what to load."
                "- Returns: new module.", 2, false) {
    auto *import = (Import *) argv[0];
    auto *spec = (ImportSpec *) argv[1];
    Code *code = nullptr;
    Module *module = nullptr;
    Frame *frame;
    argon::lang::Compiler compiler;
    std::filebuf infile;

    if (!AR_TYPEOF(import, type_import_))
        return ErrorFormat(&error_type_error, "expected Import as first arg, found '%s'", AR_TYPE_NAME(import));

    if (!AR_TYPEOF(spec, type_import_spec_))
        return ErrorFormat(&error_type_error, "expected ImportSpec as second arg, found '%s'", AR_TYPE_NAME(spec));

    if (infile.open((char *) spec->origin->buffer, std::ios::in)) {
        std::istream is(&infile);
        code = compiler.Compile(&is);
        infile.close();
    }

    if (code != nullptr) {
        if ((module = ModuleNew((const char *) spec->name->buffer, "")) == nullptr)
            goto error;

        if (!ModuleAddProperty(module, "__spec", spec, MODULE_ATTRIBUTE_PUB_CONST))
            goto error;

        if (!MapInsert(import->modules, spec->name, module))
            goto error;

        if ((frame = FrameNew(code, module->module_ns, nullptr)) == nullptr)
            goto error;

        Release(Eval(GetRoutine(), frame));
        FrameDel(frame);

        if (IsPanicking())
            goto error;

        return module;
    }

    error:
    Release(code);
    Release(module);
    MapRemove(import->modules, spec->name);
    return nullptr;
}

// LOCATORS

struct Builtins {
    const char *name;

    InitBuiltins init;
};

Function *FindFuncFromNative(List *functions, NativeFunc *native) {
    ArObject *iter;
    Function *tmp;

    if ((iter = IteratorGet(functions)) == nullptr)
        return nullptr;

    while ((tmp = (Function *) IteratorNext(iter)) != nullptr) {
        if (!AR_TYPEOF(tmp, type_function_))
            continue;

        if (tmp->IsNative() && tmp->native_fn == native->func) {
            Release(iter);
            return tmp;
        }

        Release(tmp);
    }

    Release(iter);
    return nullptr;
}

ARGON_FUNCTION5(import_, builtins_locator,
                "Locate built-in modules."
                ""
                "- Parameters:"
                "   - import: import instance."
                "   - name: module name/path."
                "   - package: nil."
                "- Returns: ImportSpec instance if module was found, otherwise nil.", 3, false) {
    static Builtins builtins[] = {{"builtins", argon::module::BuiltinsNew},
                                  {"io", argon::module::io::IONew},
                                  {"math", argon::module::MathNew},
                                  {"runtime", argon::module::RuntimeNew}};
    ImportSpec *spec = nullptr;
    Import *import;
    Function *loader;
    String *name;

    if (!AR_TYPEOF(argv[0], type_import_))
        return ErrorFormat(&error_type_error, "expected 'Import' instance as first param, found '%s'",
                           AR_TYPE_NAME(argv[0]));

    if (!AR_TYPEOF(argv[1], type_string_) && !AR_TYPEOF(argv[1], type_nil_))
        return ErrorFormat(&error_type_error, "expected 'string' as second param, found '%s'", AR_TYPE_NAME(argv[1]));

    import = (Import *) argv[0];
    name = (String *) argv[1];

    for (auto &builtin : builtins) {
        if (StringEq(name, (unsigned char *) builtin.name, strlen(builtin.name))) {
            if ((loader = FindFuncFromNative(import->loaders, &import_builtins_loader_)) == nullptr)
                return nullptr;

            spec = ImportSpecNew(name, nullptr, nullptr, loader);
            Release(loader);

            if (spec == nullptr)
                return nullptr;

            spec->initfn = builtin.init;
            break;
        }
    }

    return spec;
}

String *FindSource(String *package, String *mod_path, Tuple *exts) {
    String *path;
    String *file;

    if ((path = StringConcat(package, mod_path)) == nullptr)
        return nullptr;

    for (ArSize i = 0; i < exts->len; i++) {
        if ((file = StringConcat(path, (String *) exts->objects[i])) == nullptr) {
            Release(path);
            return nullptr;
        }

        std::ifstream infile((char *) file->buffer);

        if (infile.good()) {
            infile.close();
            Release(path);
            return file;
        }

        infile.close();
        Release(file);
    }

    Release(path);
    return nullptr;
}

String *GetModuleName(String *path, String *sep) {
    ArSSize last_sep;

    if ((last_sep = StringRFind(path, sep)) > 0)
        return StringSubs(path, last_sep + sep->len, 0);

    return IncRef(path);
}

String *FindSourceInPaths(Import *import, String *mod_path) {
    String *file = nullptr;
    String *path;

    ArObject *iter;

    if ((iter = IteratorGet(import->paths)) == nullptr)
        return nullptr;

    while ((path = (String *) IteratorNext(iter)) != nullptr) {
        if (!AR_TYPEOF(path, type_string_)) {
            Release(path);
            continue;
        }

        if ((file = FindSource(path, mod_path, import->extensions)) == nullptr) {
            if (IsPanicking()) {
                Release(path);
                Release(iter);
                return nullptr;
            }
        }

        Release(path);
    }

    Release(iter);
    return file;
}

ImportSpec *SourceLocator(Import *import, String *name, String *package) {
    ImportSpec *spec = nullptr;
    String *file = nullptr;
    String *mod_package = nullptr;

    String *mod_name;
    String *mod_sep;
    String *mod_path;

    Function *loader;

    ArSSize last_sep;

    if ((loader = FindFuncFromNative(import->loaders, &import_source_loader_)) == nullptr)
        return nullptr;

    if ((mod_sep = StringIntern("::")) == nullptr)
        return nullptr;

    // Convert import expression to OS path e.g. x::y::z -> x/y/z
    if ((mod_path = StringReplaceAll(name, mod_sep, import->path_sep)) == nullptr) {
        Release(mod_sep);
        return nullptr;
    }

    if ((mod_name = GetModuleName(name, mod_sep)) == nullptr) {
        Release(mod_sep);
        Release(mod_path);
        return nullptr;
    }

    if (package != nullptr)
        file = FindSource(package, mod_path, import->extensions);

    if (file == nullptr)
        file = FindSourceInPaths(import, mod_path);

    if (file != nullptr) {
        // Extract origin package
        last_sep = StringRFind(file, import->path_sep);
        if (last_sep > -1)
            mod_package = StringSubs(file, 0, last_sep + 1);
        else
            mod_package = StringIntern("");

        if (mod_package != nullptr)
            spec = ImportSpecNew(mod_name, mod_package, file, loader);

    }

    Release(file);
    Release(mod_package);
    Release(mod_sep);
    Release(mod_name);
    Release(mod_path);
    return spec;
}

ARGON_FUNCTION5(import_, source_locator,
                "Locate external modules."
                ""
                "- Parameters:"
                "   - import: import instance."
                "   - name: module name/path."
                "   - package: path from which to start with the search OR nil."
                "- Returns: ImportSpec instance if module was found, otherwise nil.", 3, false) {
    Import *import;
    String *name;
    String *package = nullptr;

    if (!AR_TYPEOF(argv[0], type_import_))
        return ErrorFormat(&error_type_error, "expected 'Import' instance as first param, found '%s'",
                           AR_TYPE_NAME(argv[0]));

    if (!AR_TYPEOF(argv[1], type_string_) && !AR_TYPEOF(argv[1], type_nil_))
        return ErrorFormat(&error_type_error, "expected 'string' as second param, found '%s'", AR_TYPE_NAME(argv[1]));

    if (!IsNull(argv[2])) {
        if (!AR_TYPEOF(argv[2], type_string_))
            return ErrorFormat(&error_type_error, "expected 'string' as third param, found '%s'",
                               AR_TYPE_NAME(argv[2]));

        package = (String *) argv[2];
    }

    import = (Import *) argv[0];
    name = (String *) argv[1];

    return SourceLocator(import, name, package);
}

bool AddNativeFunction(List *dst, NativeFunc *func) {
    Function *fn;
    bool ok;

    if ((fn = FunctionNew(nullptr, func)) == nullptr)
        return false;

    ok = ListAppend(dst, fn);
    Release(fn);

    return ok;
}

Import *argon::vm::ImportNew() {
#define ADD_PATH_SEP(string)                            \
    if((imp->path_sep = StringIntern(string))==nullptr) \
        goto error

#define ADD_LOCATOR(locator)                            \
    if(!AddNativeFunction(imp->locators, &(locator)))   \
        goto error

#define ADD_LOADER(loader)                          \
    if(!AddNativeFunction(imp->loaders, &(loader))) \
        goto error

    auto imp = ArObjectGCNew<Import>(&type_import_);

    if (imp != nullptr) {
        if ((imp->modules = MapNew()) == nullptr)
            goto error;

        if ((imp->paths = ListNew()) == nullptr)
            goto error;

        if ((imp->extensions = TupleNew(3)) == nullptr)
            goto error;

        if ((imp->locators = ListNew()) == nullptr)
            goto error;

        if ((imp->loaders = ListNew()) == nullptr)
            goto error;

        ADD_LOCATOR(import_builtins_locator_);
        ADD_LOCATOR(import_source_locator_);

        ADD_LOADER(import_builtins_loader_);
        ADD_LOADER(import_source_loader_);

        // Setup string intern
        Release(StringIntern("::"));

        TupleInsertAt(imp->extensions, 0, StringIntern(".ar"));
        TupleInsertAt(imp->extensions, 1, StringIntern(".arc"));

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
        ADD_PATH_SEP("\\");
        TupleInsertAt(imp->extensions,2, StringIntern(".dll"));
#elif defined(__APPLE__)
        ADD_PATH_SEP("/");
        TupleInsertAt(imp->extensions, 2, StringIntern(".dylib"));
#else
        ADD_PATH_SEP("/");
        TupleInsertAt(imp->extensions, 2, StringIntern(".so"));
#endif
    }

    return imp;

    error:
    Release(imp);
    return nullptr;
#undef ADD_LOADER
#undef ADD_LOCATOR
#undef ADD_PATH_SEP
}

bool argon::vm::ImportAddPath(Import *import, const char *path) {
    String *tmp;
    bool ok;

    if ((tmp = StringNew(path)) == nullptr)
        return false;

    ok = ImportAddPath(import, tmp);
    Release(tmp);

    return ok;
}

bool argon::vm::ImportAddPath(Import *import, String *path) {
    String *to_add = IncRef(path);
    bool ok;

    if (!StringEndsWith(path, import->path_sep)) {
        Release(to_add);
        if ((to_add = StringConcat(path, import->path_sep)) == nullptr)
            return false;
    }

    ok = ListAppend(import->paths, to_add);
    Release(to_add);

    return ok;
}

bool argon::vm::ImportAddPaths(Import *import, List *paths) {
    ArObject *iter;
    ArObject *tmp;

    if ((iter = IteratorGet(paths)) == nullptr)
        return false;

    while ((tmp = IteratorNext(iter)) != nullptr) {
        if (!AR_TYPEOF(tmp, type_string_))
            goto error;

        if (!ImportAddPath(import, (String *) tmp))
            goto error;

        Release(tmp);
    }

    Release(iter);
    return true;

    error:
    Release(tmp);
    Release(iter);
    return false;
}

argon::object::Module *argon::vm::ImportAddModule(Import *import, const char *name) {
    String *key;
    Module *module;

    if ((key = StringIntern(name)) == nullptr)
        return nullptr;

    // Retrieve from cache
    if ((module = (Module *) MapGet(import->modules, key)) != nullptr) {
        Release(key);
        return module;
    }

    // Create new empty module
    if ((module = ModuleNew(name, nullptr)) == nullptr)
        return nullptr;

    if (!ModuleAddProperty(module, "__spec", NilVal, MODULE_ATTRIBUTE_PUB_CONST)) {
        Release(key);
        Release(module);
        return nullptr;
    }

    if (!MapInsert(import->modules, key, module))
        Release((ArObject **) &module);

    Release(key);
    return module;
}

argon::object::Module *argon::vm::ImportModule(Import *import, const char *name) {
    String *key;
    Module *mod;

    if ((key = StringNew(name)) == nullptr)
        return nullptr;

    mod = ImportModule(import, key);
    Release(key);

    return mod;
}

ImportSpec *Locate(Import *import, String *name, String *package) {
    ArObject *args[3] = {};
    ArObject *iter;
    Function *fn;

    ImportSpec *ret = nullptr;

    if ((iter = IteratorGet(import->locators)) == nullptr)
        return nullptr;

    while (IsNull(ret) && (fn = (Function *) IteratorNext(iter)) != nullptr) {
        Release(ret);

        if (!AR_TYPEOF(fn, type_function_))
            continue;

        if (fn->IsNative()) {
            args[0] = import;
            args[1] = name;
            args[2] = package;
            if ((ret = (ImportSpec *) fn->native_fn(nullptr, args, 3)) == nullptr) {
                if (IsPanicking())
                    goto error;
            }
        } else {
            // TODO: Native Locate
        }

        Release(fn);
    }

    Release(iter);

    if (!IsNull(ret)) {
        if (!AR_TYPEOF(ret, type_import_spec_)) {
            Release(ret);
            return (ImportSpec *) ErrorFormat(&error_type_error,
                                              "locator functions MUST returns 'ImportSpec' instance, not '%s'",
                                              AR_TYPE_NAME(ret));
        }
    } else Release((ArObject **) &ret);

    return ret;

    error:
    Release(fn);
    Release(iter);
    return nullptr;
}

Module *Load(Import *import, ImportSpec *spec) {
    ArObject *args[2] = {};
    Function *fn = spec->loader;
    Module *module = nullptr;

    if (fn->IsNative()) {
        args[0] = import;
        args[1] = spec;
        if ((module = (Module *) fn->native_fn(nullptr, args, 2)) == nullptr)
            return nullptr;
    } else {
        // TODO: Native Loader
    }

    if (IsNull(module))
        Release((ArObject **) &module);

    return module;
}

argon::object::Module *argon::vm::ImportModule(Import *import, String *name, String *package) {
    ImportSpec *spec;
    Module *module;

    // Retrieve from cache
    if ((module = (Module *) MapGet(import->modules, name)) != nullptr)
        return module;

    if ((spec = Locate(import, name, package)) == nullptr)
        return (Module *) ErrorFormat(&error_module_notfound, "No module named '%s'", name->buffer);

    module = Load(import, spec);

    Release(spec);
    return module;
}