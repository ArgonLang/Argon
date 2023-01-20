// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <Windows.h>

#else

#include <dlfcn.h>

#endif

#include <vm/datatype/error.h>

#include "dlwrap.h"

using namespace argon::vm::datatype;
using namespace argon::vm::importer;

DLHandle argon::vm::importer::OpenLibrary(const char *path, Error **out_error) {
#ifdef _ARGON_PLATFORM_WINDOWS
    HMODULE handle;

    if ((handle = LoadLibraryA(path)) == nullptr) {
        auto *winerr = ErrorGetMsgFromWinErr();

        *out_error = ErrorNewFormat(kModuleImportError[0],
                                    "NativeImportError: %s",
                                    winerr != nullptr ? ARGON_RAW_STRING(winerr) : (unsigned char *) "");

        Release(winerr);

        return nullptr;
    }

    return (DLHandle) handle;
#else
    DLHandle handle;

    *out_error = nullptr;

    if ((handle = dlopen(path, RTLD_NOW)) == nullptr) {
        *out_error = ErrorNewFormat(kModuleImportError[0], "NativeImportError: %s", dlerror());

        return nullptr;
    }

    return handle;
#endif
}

DLHandle argon::vm::importer::LoadSymbol(DLHandle handle, const char *sym_name) {
#ifdef _ARGON_PLATFORM_WINDOWS
    return (DLHandle) GetProcAddress((HMODULE) handle, sym_name);
#else
    return dlsym(handle, sym_name);
#endif
}

int argon::vm::importer::CloseLibrary(DLHandle handle, datatype::Error **out_error) {
#ifdef _ARGON_PLATFORM_WINDOWS
    if (FreeLibrary((HMODULE) handle) == 0) {
        auto *winerr = ErrorGetMsgFromWinErr();

        *out_error = ErrorNewFormat(kModuleImportError[0],
                                    "UnloadNativeModule: %s",
                                    winerr != nullptr ? ARGON_RAW_STRING(winerr) : (unsigned char *) "");

        Release(winerr);

        return 1;
    }
#else
    *out_error = nullptr;

    if (dlclose(handle) != 0) {
        *out_error = ErrorNewFormat(kModuleImportError[0], "UnloadNativeModule: %s", dlerror());

        return 1;
    }
#endif

    return 0;
}