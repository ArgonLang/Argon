// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_SUPPORT_NT_NT_H_
#define ARGON_VM_SUPPORT_NT_NT_H_

#include <argon/vm/datatype/arobject.h>

namespace argon::vm::support::nt {
    argon::vm::datatype::ArObject *GetLogin();

    int GetExecutablePath(char *out_buf, int size);

} // namespace argon::vm::support::nt

#endif // !ARGON_VM_SUPPORT_NT_NT_H_