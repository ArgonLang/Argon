// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <fstream>

#include <argon/util/macros.h>

#include <argon/lang/compiler_wrapper.h>

#include <argon/vm/runtime.h>

#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/nil.h>

#include <argon/vm/mod/modules.h>

#include <argon/vm/importer/dlwrap.h>
#include <argon/vm/importer/import.h>

using namespace argon::vm::datatype;
using namespace argon::vm::importer;

// PROTOTYPES

bool AddModule2Cache(Import *imp, String *name, Module *mod);

bool ModuleDLHandleUnload(Module *self);

Function *FindNativeFnInstance(List *, const FunctionDef *);

String *FindSource(const Import *imp, String *package_path, String *mod_path, String *mod_name);

String *FindSourceInit(const Import *imp, String *path, String *mod_name);

String *FindSourceInPaths(const Import *imp, String *mod_path, String *mod_name);

String *GetModuleName(String *path, const String *sep);

void DelModuleFromCache(Import *imp, String *name);

// EOF

ArObject *import_compare(const ArObject *self, const ArObject *other, CompareMode mode) {
    if (mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(self == other);
}

bool import_dtor(Import *self) {
    Release(self->loaders);
    Release(self->locators);
    Release(self->paths);
    Release(self->path_sep);

    self->module_cache.Finalize([](ImportModuleCacheEntry *entry) {
        Release(entry->key);
        Release(entry->value);
    });

    self->lock.~mutex();

    return true;
}

void import_trace(Import *self, Void_UnaryOp trace) {
    std::unique_lock _(self->lock);

    for (auto *cursor = self->module_cache.iter_begin; cursor != nullptr; cursor = cursor->iter_next) {
        trace(cursor->value);
    }
}

TypeInfo ImportType = {
        AROBJ_HEAD_INIT_TYPE,
        "Import",
        nullptr,
        nullptr,
        sizeof(Import),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) import_dtor,
        (TraceOp) import_trace,
        nullptr,
        nullptr,
        (CompareOp) import_compare,
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
const TypeInfo *argon::vm::importer::type_import_ = &ImportType;

// LOADERS

ARGON_FUNCTION(import_builtins_loader, builtins_loader,
               "Load built-in modules.\n"
               "\n"
               "- Parameters:\n"
               "   - import: Import instance.\n"
               "   - spec: ImportSpec describing what to load.\n"
               "- Returns: New module.\n",
               ": import, : spec", false, false) {
    auto *imp = (Import *) args[0];
    auto *spec = (ImportSpec *) args[1];

    auto *mod = ModuleNew(spec->init);
    if (mod == nullptr)
        return nullptr;

    if (!ModuleAddObject(mod, "__spec", (ArObject *) spec, MODULE_ATTRIBUTE_DEFAULT | AttributeFlag::NON_COPYABLE)) {
        Release(mod);
        return nullptr;
    }

    if (!AddModule2Cache(imp, spec->name, mod)) {
        Release(mod);
        return nullptr;
    }

    return (ArObject *) mod;
}

ARGON_FUNCTION(import_native_loader, native_loader,
               "Load external modules from native library.\n"
               "\n"
               "- Parameters:\n"
               "   - import: Import instance.\n"
               "   - spec: ImportSpec describing what to load.\n"
               "- Returns: New module.\n",
               ": import, : spec", false, false) {
    auto *spec = (ImportSpec *) args[1];
    Error *error;
    Module *mod;

    DLHandle handle;
    ModuleNativeInitFn symbol;

    if ((handle = OpenLibrary((const char *) ARGON_RAW_STRING(spec->origin), &error)) == DLHandleError) {
        argon::vm::Panic((ArObject *) error);
        Release(error);
        return nullptr;
    }

    if ((symbol = (ModuleNativeInitFn) LoadSymbol(handle, kModuleInitFnName)) == DLHandleError) {
        if (!CloseLibrary(handle, &error)) {
            argon::vm::Panic((ArObject *) error);
            Release(error);
        } else
            ErrorFormat(kModuleImportError[0], kModuleImportError[5], kModuleInitFnName);

        return nullptr;
    }

    if ((mod = ModuleNew(symbol())) == nullptr) {
        if (!CloseLibrary(handle, &error)) {
            argon::vm::Panic((ArObject *) error);
            Release(error);
        }

        return nullptr;
    }

    ModuleSetDLHandle(mod, ModuleDLHandleUnload, (uintptr_t) handle);

    if (!ModuleAddObject(mod, "__spec", (ArObject *) spec, MODULE_ATTRIBUTE_DEFAULT | AttributeFlag::NON_COPYABLE)) {
        Release(mod);
        return nullptr;
    }

    return (ArObject *) mod;
}

ARGON_FUNCTION(import_source_loader, source_loader,
               "Load external modules from sources.\n"
               "\n"
               "- Parameters:\n"
               "   - import: Import instance.\n"
               "   - spec: ImportSpec describing what to load.\n"
               "- Returns: New module.\n",
               ": import, : spec", false, false) {
    argon::lang::CompilerWrapper compiler;
    auto *imp = (Import *) args[0];
    auto *spec = (ImportSpec *) args[1];
    Code *code;
    FILE *infile;

    if ((infile = fopen((const char *) ARGON_RAW_STRING(spec->origin), "r")) == nullptr)
        return nullptr;

    if ((code = compiler.Compile((const char *) ARGON_RAW_STRING(spec->name), infile)) == nullptr) {
        fclose(infile);
        return nullptr;
    }

    fclose(infile);

    auto *mod = ModuleNew(spec->name, code->doc);
    if (mod == nullptr) {
        Release(code);
        return nullptr;
    }

    if (!ModuleAddObject(mod, "__spec", (ArObject *) spec, MODULE_ATTRIBUTE_DEFAULT | AttributeFlag::NON_COPYABLE)) {
        Release(mod);
        return nullptr;
    }

    auto *result = argon::vm::Eval(imp->context, code, mod->ns);

    Release(code);

    if (!result->success) {
        argon::vm::Panic(result->value);

        Release(result);
        Release(mod);
        return nullptr;
    }

    Release(result);

    return (ArObject *) mod;
}

// LOCATORS

ARGON_FUNCTION(import_builtins_locator, builtins_locator,
               "Locate built-in modules.\n"
               "\n"
               "- Parameters:\n"
               "   - import: Import instance.\n"
               "   - name: Module name/path.\n"
               "   - hint: ImportSpec | nil.\n"
               "- Returns: ImportSpec instance if module was found, otherwise nil.\n",
               ": import, s: name, : hint", false, false) {
    static const ModuleInit *builtins[] = {
            argon::vm::mod::module_builtins_,
            argon::vm::mod::module_chrono_,
            argon::vm::mod::module_gc_,
            argon::vm::mod::module_io_,
            argon::vm::mod::module_limits_,
            argon::vm::mod::module_os_,
            argon::vm::mod::module_runtime_,
            argon::vm::mod::module_socket_
    };
    ImportSpec *spec = nullptr;

    for (auto &builtin: builtins) {
        if (StringEqual((String *) args[1], builtin->name)) {
            auto *loader = FindNativeFnInstance(((Import *) *args)->loaders, &import_builtins_loader);

            assert(loader != nullptr);

            spec = ISpecNew((String *) args[1], nullptr, nullptr, loader);
            if (spec != nullptr)
                spec->init = builtin;

            Release(loader);

            break;
        }
    }

    return (ArObject *) spec;
}

ARGON_FUNCTION(import_source_locator, source_locator,
               "Locate external modules.\n"
               "\n"
               "- Parameters:\n"
               "   - import: Import instance.\n"
               "   - name: Module name/path.\n"
               "   - hint: ImportSpec | nil.\n"
               "- Returns: ImportSpec instance if module was found, otherwise nil.\n",
               ": import, s: name, : hint", false, false) {
    auto *imp = (Import *) args[0];
    auto *mod_path = (String *) args[1];
    const auto *hint = (const ImportSpec *) args[2];

    Function *loader;
    ImportSpec *ret = nullptr;
    String *file = nullptr;
    String *mod_package = nullptr;
    String *mod_name;

    loader = FindNativeFnInstance(imp->loaders, &import_source_loader);
    if (loader == nullptr) {
        ErrorFormat(kModuleImportError[0], kModuleImportError[3]);
        return nullptr;
    }

    mod_name = GetModuleName((String *) args[1], imp->path_sep);

    if (hint != nullptr && hint->path != nullptr)
        file = FindSource(imp, hint->path, mod_path, mod_name);

    if (file == nullptr)
        file = FindSourceInPaths(imp, mod_path, mod_name);

    if (file != nullptr) {
        auto last_sep = StringRFind(file, imp->path_sep);

        if (last_sep > -1)
            mod_package = StringSubs(file, 0, last_sep + 1);
        else
            mod_package = StringIntern("");

        if (StringEndswith(file, kExtension[(sizeof(kExtension) / sizeof(void *)) - 1])) {
            Release(loader);

            loader = FindNativeFnInstance(imp->loaders, &import_native_loader);
            if (loader == nullptr) {
                Release(mod_name);
                Release(mod_package);
                Release(loader);

                ErrorFormat(kModuleImportError[0], kModuleImportError[4]);

                return nullptr;
            }
        }

        if (mod_package != nullptr)
            ret = ISpecNew(mod_name, mod_package, file, loader);
    }

    Release(mod_name);
    Release(mod_package);
    Release(loader);

    return (ArObject *) ret;
}

bool AddModule2Cache(Import *imp, String *name, Module *mod) {
    ImportModuleCacheEntry *entry;
    ArObject *value;

    value = (ArObject *) mod;
    if (mod == nullptr)
        value = IncRef((ArObject *) Nil);

    if ((entry = imp->module_cache.Lookup(name)) != nullptr) {
        Release(entry->value);
        entry->value = IncRef(value);

        if (mod == nullptr)
            Release(value);

        return true;
    }

    if ((entry = imp->module_cache.AllocHEntry()) == nullptr) {
        if (mod == nullptr)
            Release(value);

        return false;
    }

    entry->key = IncRef(name);
    entry->value = IncRef(value);

    if (!imp->module_cache.Insert(entry)) {
        Release(name);

        if (mod == nullptr)
            Release(value);

        imp->module_cache.FreeHEntry(entry);

        return false;
    }

    if (mod == nullptr)
        Release(value);

    return true;
}

bool AddNativeFunction(List *dest, const FunctionDef *func) {
    Function *fn;

    if ((fn = FunctionNew(func, nullptr, nullptr)) == nullptr)
        return false;

    auto ok = ListAppend(dest, (ArObject *) fn);

    Release(fn);

    return ok;
}

bool ModuleDLHandleUnload(Module *self) {
    Error *error;

    if (CloseLibrary((DLHandle) self->_dlhandle, &error) != 0)
        return true;

    Release(error); // Ignore error
    return false;
}

bool argon::vm::importer::ImportAddPath(Import *imp, const char *path) {
    auto *s_path = StringNew(path);
    bool ok = false;

    if (s_path != nullptr) {
        ok = ListAppend(imp->paths, (ArObject *) s_path);

        Release(s_path);
    }

    return ok;
}

bool argon::vm::importer::ImportAddPath(Import *imp, String *path) {
    return ListAppend(imp->paths, (ArObject *) path);
}

bool argon::vm::importer::ImportAddPaths(Import *imp, datatype::List *paths) {
    ArObject *iter;
    ArObject *tmp;

    if ((iter = IteratorGet((ArObject *) paths, false)) == nullptr)
        return false;

    while ((tmp = IteratorNext(iter)) != nullptr) {
        if (!AR_TYPEOF(tmp, type_string_)) {
            Release(tmp);
            continue;
        }

        if (!ImportAddPath(imp, (String *) tmp)) {
            Release(tmp);
            Release(iter);

            return false;
        }

        Release(tmp);
    }

    Release(iter);

    return true;
}

Function *FindNativeFnInstance(List *search_list, const FunctionDef *def) {
    ArObject *iter;
    Function *tmp;

    if ((iter = IteratorGet((ArObject *) search_list, false)) == nullptr)
        return nullptr;

    while ((tmp = (Function *) IteratorNext(iter)) != nullptr) {
        if (tmp->IsNative() && tmp->native == def->func) {
            Release(iter);
            return tmp;
        }

        Release(tmp);
    }

    Release(iter);
    return nullptr;
}

Import *argon::vm::importer::ImportNew(Context *context) {
#define ADD_FUNC(dest, fdef)                    \
    do {                                        \
        if(!AddNativeFunction(dest, &(fdef)))   \
            goto ERROR;                         \
    } while(0)

    auto *imp = MakeGCObject<Import>(type_import_, true);

    if (imp == nullptr)
        return nullptr;

    memory::MemoryZero(&imp->module_cache, sizeof(ImportModuleCache));

    imp->context = context;

    imp->locators = nullptr;
    imp->paths = nullptr;
    imp->path_sep = nullptr;

    if ((imp->loaders = ListNew()) == nullptr)
        goto ERROR;

    if ((imp->locators = ListNew()) == nullptr)
        goto ERROR;

    if ((imp->paths = ListNew()) == nullptr)
        goto ERROR;

    if ((imp->path_sep = StringIntern(_ARGON_PLATFORM_PATHSEP)) == nullptr)
        goto ERROR;

    if (!imp->module_cache.Initialize())
        goto ERROR;

    ADD_FUNC(imp->loaders, import_builtins_loader);
    ADD_FUNC(imp->loaders, import_source_loader);
    ADD_FUNC(imp->loaders, import_native_loader);

    ADD_FUNC(imp->locators, import_builtins_locator);
    ADD_FUNC(imp->locators, import_source_locator);

    new(&imp->lock)std::mutex();

    return imp;

    ERROR:
    Release(imp);
    return nullptr;
#undef ADD_FUNC
}

ImportSpec *Locate(Import *imp, String *name, ImportSpec *hint) {
    ArObject *args[] = {(ArObject *) imp, (ArObject *) name, (ArObject *) hint};
    ArObject *iter;
    Function *fn;
    ImportSpec *spec;

    if ((iter = IteratorGet((ArObject *) imp->locators, false)) == nullptr)
        return nullptr;

    spec = nullptr;
    while ((fn = (Function *) IteratorNext(iter)) != nullptr) {
        if (fn->IsNative())
            spec = (ImportSpec *) fn->native((ArObject *) fn, nullptr, args, nullptr, 0);
        else
            assert(false);

        Release(fn);

        if (argon::vm::IsPanicking()) {
            Release(iter);
            return nullptr;
        }

        if (spec != nullptr) {
            if (!AR_TYPEOF(spec, type_import_spec_)) {
                ErrorFormat(kTypeError[0], "invalid return value from import locator '%s' expected %s, got '%s'",
                            ARGON_RAW_STRING(fn->name), type_import_spec_->name, AR_TYPE_NAME(spec));

                Release((ArObject **) &spec);
            }

            break;
        }
    }

    Release(iter);

    return spec;
}

Module *argon::vm::importer::ImportAdd(Import *imp, const char *name) {
    ImportModuleCacheEntry *entry;
    String *ar_name;

    if ((ar_name = StringNew(name)) == nullptr)
        return nullptr;

    std::unique_lock _(imp->lock);

    if ((entry = imp->module_cache.Lookup(ar_name)) != nullptr) {
        assert(entry->value != nullptr);

        Release(ar_name);

        return (Module *) IncRef(entry->value);
    }

    auto *mod = ModuleNew(ar_name, nullptr);

    if (!AddModule2Cache(imp, ar_name, mod)) {
        Release(ar_name);
        Release(mod);
        return nullptr;
    }

    Release(ar_name);

    return mod;
}

Module *Load(Import *imp, ImportSpec *spec) {
    ArObject *args[] = {(ArObject *) imp, (ArObject *) spec};
    Function *fn = spec->loader;
    Module *mod;

    if (fn->IsNative())
        mod = (Module *) fn->native((ArObject *) fn, nullptr, args, nullptr, 2);
    else
        assert(false);

    if (IsNull((ArObject *) mod)) {
        Release(mod); // release nil object

        return nullptr;
    }

    if (!AR_TYPEOF(mod, type_module_)) {
        ErrorFormat(kTypeError[0], "invalid return value from import loader '%s' expected %s, got '%s'",
                    ARGON_RAW_STRING(fn->name), type_module_->name, AR_TYPE_NAME(mod));

        Release(mod);
        return nullptr;
    }

    return mod;
}

Module *argon::vm::importer::LoadModule(Import *imp, const char *name, ImportSpec *hint) {
    String *ar_name;
    Module *mod;

    if ((ar_name = StringNew(name)) == nullptr)
        return nullptr;

    mod = LoadModule(imp, ar_name, hint);

    Release(ar_name);

    return mod;
}

Module *argon::vm::importer::LoadModule(Import *imp, String *name, ImportSpec *hint) {
    ImportModuleCacheEntry *entry;

    std::unique_lock lock(imp->lock);

    if ((entry = imp->module_cache.Lookup(name)) != nullptr) {
        if (!AR_TYPEOF(entry->value, type_module_)) {
            ErrorFormat(kModuleImportError[0], kModuleImportError[2], ARGON_RAW_STRING(name));
            return nullptr;
        }

        return (Module *) IncRef(entry->value);
    }

    auto *spec = Locate(imp, name, hint);
    if (spec == nullptr) {
        ErrorFormat(kModuleImportError[0], kModuleImportError[0], ARGON_RAW_STRING(name));
        return nullptr;
    }

    if (!AddModule2Cache(imp, name, nullptr)) {
        Release(spec);
        return nullptr;
    }

    lock.unlock();

    auto *mod = Load(imp, spec);

    lock.lock();

    if (mod != nullptr) {
        if (!AddModule2Cache(imp, name, mod)) {
            Release(mod);
            return nullptr;
        }
    } else
        DelModuleFromCache(imp, name);

    return mod;
}

String *FindSource(const Import *imp, String *package_path, String *mod_path, String *mod_name) {
    String *ret;
    String *path;

    std::ifstream infile;

    if (!StringEndswith(package_path, imp->path_sep)) {
        if ((ret = StringConcat(package_path, imp->path_sep)) == nullptr)
            return nullptr;
    } else
        ret = IncRef(package_path);

    if ((path = StringConcat(ret, mod_path)) == nullptr) {
        Release(ret);

        return nullptr;
    }

    Release((ArObject **) &ret);

    for (auto *ext: kExtension) {
        if ((ret = StringConcat(path, ext, strlen(ext))) == nullptr) {
            Release(path);
            return nullptr;
        }

        infile = std::ifstream((char *) ARGON_RAW_STRING(ret));

        if (infile.good()) {
            infile.close();

            Release(path);

            return ret;
        }

        infile.close();
        Release((ArObject **) &ret);
    }

    if (ret == nullptr)
        ret = FindSourceInit(imp, path, mod_name);

    Release(path);

    return ret;
}

String *FindSourceInit(const Import *imp, String *path, String *mod_name) {
    String *file;
    String *tmp;

    if ((tmp = StringConcat(imp->path_sep, mod_name)) == nullptr)
        return nullptr;

    file = StringConcat(path, tmp);

    Release(tmp);

    if (file == nullptr)
        return nullptr;

    tmp = file;

    for (auto *ext: kExtension) {
        if ((file = StringConcat(tmp, ext, strlen(ext))) == nullptr) {
            Release(tmp);

            return nullptr;
        }

        std::ifstream infile((char *) ARGON_RAW_STRING(file));

        if (infile.good()) {
            infile.close();

            Release(tmp);

            return file;
        }

        infile.close();

        Release(file);
    }

    Release(tmp);

    return nullptr;
}

String *FindSourceInPaths(const Import *imp, String *mod_path, String *mod_name) {
    String *file = nullptr;
    String *path;

    ArObject *iter;
    if ((iter = IteratorGet((ArObject *) imp->paths, false)) == nullptr)
        return nullptr;

    while ((path = (String *) IteratorNext(iter)) != nullptr) {
        file = FindSource(imp, path, mod_path, mod_name);
        if (file == nullptr && argon::vm::IsPanicking()) {
            Release(path);
            Release(iter);
            return nullptr;
        }

        Release(path);

        if (file != nullptr)
            break;
    }

    Release(iter);

    return file;
}

String *GetModuleName(String *path, const String *sep) {
    ArSSize last_sep;

    last_sep = StringRFind(path, sep);
    if (last_sep > 0)
        return StringSubs(path, last_sep + ARGON_RAW_STRING_LENGTH(sep), 0);

    return IncRef(path);
}

void DelModuleFromCache(Import *imp, String *name) {
    auto *entry = imp->module_cache.Remove(name);

    Release(entry->key);
    Release(entry->value);

    imp->module_cache.FreeHEntry(entry);
}