// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <utils/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <lmcons.h>
#include <windows.h>

#include <object/datatype/string.h>

#include "nt.h"

using namespace argon::object;
using namespace argon::module::nt;

ArObject *argon::module::nt::GetLogin() {
    char username[UNLEN + 1];
    DWORD user_len = sizeof(username);

    if (GetUserNameA(username, &user_len) != 0)
        return StringNew(username, user_len - 1);

    // TODO: GetLastError
    return nullptr;
}

#endif
