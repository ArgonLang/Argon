// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <Windows.h>

#undef CONST
#undef FASTCALL
#undef Yield

#include <vm/datatype/arobject.h>
#include <vm/datatype/error.h>

#include <vm/runtime.h>

using namespace argon::vm::datatype;

argon::vm::datatype::Error *argon::vm::datatype::ErrorNewFromWinErr() {
    Error *error;
    String *msg;

    if ((msg = ErrorGetMsgFromWinErr()) == nullptr)
        return nullptr;

    if ((error = ErrorNew(kOSError[0], msg)) == nullptr) {
        Release(msg);
        return nullptr;
    }

    Release(msg);
    return error;
}

argon::vm::datatype::String *argon::vm::datatype::ErrorGetMsgFromWinErr() {
    LPSTR mbuffer = nullptr;
    String *msg;

    DWORD eid;

    if ((eid = ::GetLastError()) != 0) {
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                     FORMAT_MESSAGE_FROM_SYSTEM |
                                     FORMAT_MESSAGE_IGNORE_INSERTS |
                                     FORMAT_MESSAGE_MAX_WIDTH_MASK,
                                     nullptr,
                                     eid,
                                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                     (LPSTR) &mbuffer,
                                     0,
                                     nullptr);

        if (size == 0)
            msg = StringNew("FormatMessageA failed. Could not get error message from Windows");
        else
            msg = StringNew(mbuffer, size);

        LocalFree(mbuffer);
    } else
        msg = StringIntern("operation completed successfully");

    return msg;
}

void argon::vm::datatype::ErrorFromWinErr() {
    auto *error = ErrorNewFromWinErr();

    if (error != nullptr)
        argon::vm::Panic((ArObject *) error);

    Release(error);
}

#endif

