// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/datatype/error.h>

#include "dlwrap.h"

#ifdef _ARGON_PLATFORM_WINDOWS
// TODO Windows
#else

#include <dlfcn.h>

#endif

using namespace argon::vm::datatype;
using namespace argon::vm::importer;

DLHandle argon::vm::importer::OpenLibrary(const char *path, Error **out_error) {
    DLHandle handle;

    *out_error = nullptr;

    if ((handle = dlopen(path, RTLD_NOW)) == nullptr) {
        *out_error = ErrorNewFormat(kModuleImportError[0], "NativeImportError: %s", dlerror());

        return nullptr;
    }

    return handle;
}

DLHandle argon::vm::importer::LoadSymbol(DLHandle handle, const char *sym_name) {
    return dlsym(handle, sym_name);
}

int argon::vm::importer::CloseLibrary(DLHandle handle, datatype::Error **out_error) {
    *out_error = nullptr;

    if (dlclose(handle) != 0) {
        *out_error = ErrorNewFormat(kModuleImportError[0], "UnloadNativeModule: %s", dlerror());

        return 1;
    }

    return 0;
}