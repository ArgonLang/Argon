// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_IO_IO_H_
#define ARGON_VM_IO_IO_H_

#include <argon/util/macros.h>

#include <argon/vm/datatype/objectdef.h>

namespace argon::vm::io {

#ifdef _ARGON_PLATFORM_WINDOWS
    using IOHandle = void *;
#else
    using IOHandle = int;
#endif

    const extern datatype::TypeInfo *type_line_reader_t_;

    const extern datatype::TypeInfo *type_reader_t_;

    const extern datatype::TypeInfo *type_writer_t_;

    bool IOInit();

} // namespace argon::vm::io

#endif // !ARGON_VM_IO_IO_H_
