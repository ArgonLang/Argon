// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "util/macros.h"

#ifdef _ARGON_PLATFORM_WINDOWS

#include <lmcons.h>
#include <windows.h>

#include <vm/datatype/arstring.h>
#include <vm/datatype/error.h>

#include "nt.h"

using namespace argon::vm::datatype;
using namespace argon::vm::support::nt;

ArObject *argon::vm::support::nt::GetLogin() {
    char username[UNLEN + 1];
    DWORD user_len = sizeof(username);

    if (GetUserNameA(username, &user_len) != 0)
        return (ArObject *) StringNew(username, user_len - 1);

    ErrorFromWinErr();
    return nullptr;
}


int argon::vm::support::nt::GetExecutablePath(char *out_buf, int size) {
    if ((size = GetModuleFileNameA(nullptr, out_buf, size)) == 0)
        size = -1;

    return size;
}

#endif