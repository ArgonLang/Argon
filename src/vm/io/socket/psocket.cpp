// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <util/macros.h>

#ifndef _ARGON_PLATFORM_WINDOWS

#include "socket.h"

using namespace argon::vm::datatype;
using namespace argon::vm::io::socket;

Error *argon::vm::io::socket::ErrorNewFromSocket() {
    return ErrorNewFromErrno(errno);
}

#endif