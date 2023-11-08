// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_IO_PIPE_H_
#define ARGON_VM_IO_PIPE_H_

#include <argon/util/macros.h>

#include <argon/vm/datatype/objectdef.h>

#include <argon/vm/io/io.h>

namespace argon::vm::io {
#ifdef _ARGON_PLATFORM_WINDOWS
#ifndef O_CLOEXEC
#define O_CLOEXEC 02000000
#endif
#endif

    _ARGONAPI bool MakePipe(IOHandle *read, IOHandle *write, int flags);

    _ARGONAPI void ClosePipe(IOHandle pipe);

} // namespace argon::vm::io

#endif // !ARGON_VM_IO_PIPE_H_
