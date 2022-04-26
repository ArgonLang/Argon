// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/runtime.h>

#include "string.h"
#include "error.h"

#ifdef _ARGON_PLATFORM_WINDOWS

#include <Windows.h>

using namespace argon::object;

ArObject *argon::object::ErrorNewFromWinError() {
    LPSTR mbuffer = nullptr;
    ArObject *error;
    String *msg;
    DWORD eid;

    if ((eid = GetLastError()) != 0) {
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                     FORMAT_MESSAGE_FROM_SYSTEM |
                                     FORMAT_MESSAGE_IGNORE_INSERTS,
                                     nullptr,
                                     eid,
                                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                     (LPSTR) &mbuffer,
                                     0,
                                     nullptr);

        msg = StringNew(mbuffer, size);
        LocalFree(mbuffer);
    } else
        msg = StringIntern("");

    if (msg == nullptr)
        return nullptr;

    if ((error = ErrorNew(type_os_error_, msg)) == nullptr) {
        Release(msg);
        return nullptr;
    }

    Release(msg);
    return error;
}

ArObject *argon::object::ErrorSetFromWinError() {
    ArObject *err = ErrorNewFromWinError();

    if (err != nullptr) {
        argon::vm::Panic(err);
        Release(err);
    }

    return nullptr;
}

ArSize argon::object::ErrorGetLast() {
    return GetLastError();
}

#endif
