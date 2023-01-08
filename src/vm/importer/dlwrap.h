// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_IMPORTER_DLWRAP_H_
#define ARGON_VM_IMPORTER_DLWRAP_H_

#include <util/macros.h>

namespace argon::vm::importer {
#ifdef _ARGON_PLATFORM_WINDOWS
    // TODO: Windows
#else
    using DLHandle = void *;

    constexpr DLHandle DLHandleError = nullptr;

    DLHandle OpenLibrary(const char *path, datatype::Error **out_error);

    DLHandle LoadSymbol(DLHandle handle, const char *sym_name);

    int CloseLibrary(DLHandle handle, datatype::Error **out_error);

#endif

} // namespace argon::vm::importer

#endif // !ARGON_VM_IMPORTER_DLWRAP_H_
