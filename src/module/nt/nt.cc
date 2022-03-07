// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <utils/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <lmcons.h>
#include <Windows.h>

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

int argon::module::nt::GetExecutablePath(char* out_buf, int size) {
    if ((size = GetModuleFileNameA(nullptr, out_buf, size)) == 0)
        size = -1;

    return size;
}

#endif
