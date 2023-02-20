// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/datatype/error.h>

#include "socket.h"

using namespace argon::vm::datatype;
using namespace argon::vm::io::socket;

void argon::vm::io::socket::ErrorFromSocket() {
    ErrorFromErrno(errno);
}