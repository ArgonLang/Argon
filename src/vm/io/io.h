// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_IO_IO_H_
#define ARGON_VM_IO_IO_H_

#include <util/macros.h>

namespace argon::vm::io {

#ifdef _ARGON_PLATFORM_WINDOWS
    using IOHandle = void *;
#else
    using IOHandle = int;
#endif

    bool IOInit();

} // namespace argon::vm::io

#endif // !ARGON_VM_IO_IO_H_
