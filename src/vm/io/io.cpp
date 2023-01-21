// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "fio.h"
#include "io.h"

using namespace argon::vm::datatype;
using namespace argon::vm::io;

bool argon::vm::io::IOInit() {
    if (!TypeInit((TypeInfo *) type_file_, nullptr))
        return false;

    return true;
}