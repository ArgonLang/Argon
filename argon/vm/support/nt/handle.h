// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_SUPPORT_NT_HANDLE_H_
#define ARGON_VM_SUPPORT_NT_HANDLE_H_

#include <windows.h>

#include <argon/vm/datatype/arobject.h>

#undef CONST

namespace argon::vm::support::nt {
    struct OSHandle {
        AROBJ_HEAD;

        HANDLE handle;
    };

    _ARGONAPI extern const datatype::TypeInfo *type_oshandle_;

    _ARGONAPI OSHandle *OSHandleNew(HANDLE handle);

} // namespace argon::vm::support::nt

#endif // !ARGON_VM_SUPPORT_NT_HANDLE_H_