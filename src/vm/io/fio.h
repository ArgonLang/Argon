// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_IO_FIO_H_
#define ARGON_VM_IO_FIO_H_

#include <cstring>

#include <util/enum_bitmask.h>

#include <vm/datatype/arobject.h>

#include "io.h"

namespace argon::vm::io {
    enum class FileMode : unsigned int {
        READ = 1u,
        WRITE = 1u << 1u,
        APPEND = 1u << 2u,
    };

    enum class FileWhence {
        START,
        CUR,
        END
    };

    struct File {
        AROBJ_HEAD;

        IOHandle handle;

        FileMode mode;
    };
    extern const datatype::TypeInfo *type_file_;

    datatype::ArSize GetFd(File *file);

    datatype::ArSSize Read(File *file, unsigned char *buf, datatype::ArSize count);

    datatype::ArSSize Write(File *file, const unsigned char *buf, datatype::ArSize count);

    datatype::ArSSize WriteObject(File *file, datatype::ArObject *object);

    datatype::ArSSize WriteObjectStr(File *file, datatype::ArObject *object);

    inline datatype::ArSSize WriteString(File *file, const char *str) {
        return Write(file, (const unsigned char *) str, strlen(str));
    }

    bool GetFileSize(File *file, datatype::ArSize *out_size);

    bool Isatty(File *file);

    bool IsSeekable(File *file);

    bool Seek(File *file, datatype::ArSSize offset, FileWhence whence);

    bool Tell(File *file, datatype::ArSize *out_pos);

    File *FileNew(const char *path, FileMode mode);
} // namespace argon::vm::io

ENUMBITMASK_ENABLE(argon::vm::io::FileMode);

#endif // !ARGON_VM_IO_FIO_H_