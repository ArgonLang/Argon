// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_IMPORTER_DLWRAP_H_
#define ARGON_VM_IMPORTER_DLWRAP_H_

namespace argon::vm::importer {
    using DLHandle = void *;

    constexpr DLHandle DLHandleError = nullptr;

    DLHandle OpenLibrary(const char *path, datatype::Error **out_error);

    DLHandle LoadSymbol(DLHandle handle, const char *sym_name);

    int CloseLibrary(DLHandle handle, datatype::Error **out_error);

} // namespace argon::vm::importer

#endif // !ARGON_VM_IMPORTER_DLWRAP_H_
