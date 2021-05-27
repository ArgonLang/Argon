// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_IO_IO_H_
#define ARGON_OBJECT_IO_IO_H_

#include <object/arobject.h>
#include <utils/enum_bitmask.h>

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#define ARGON_OBJECT_IO_DEFAULT_BUFSIZE 4096

namespace argon::object::io {

    enum class FileMode : unsigned char {
        READ = 1u,
        WRITE = 1u << 1u,
        APPEND = 1u << 2u,

        // PRIVATE/NOT EXPORTED FIELDS
        _IS_TERM = 1u << 3u,
        _IS_PIPE = 1u << 4u
    };

    enum class FileBufferMode {
        NONE,
        LINE,
        BLOCK
    };

    enum class FileWhence {
        START,
        CUR,
        END
    };

    struct File : argon::object::ArObject {
        int fd;
        ArSize cur;
        FileMode mode;

        struct {
            FileBufferMode mode;
            unsigned char *buf;
            unsigned char *cur;

            long cap;
            long len;
            long wlen;
        } buffer;
    };

    extern const argon::object::TypeInfo *type_file_;

    bool Flush(File *file);

    bool Isatty(File *file);

    bool IsSeekable(File *file);

    bool Seek(File *file, ArSSize offset, FileWhence whence);

    bool SetBuffer(File *file, unsigned char *buf, ArSSize cap, FileBufferMode mode);

    File *Open(char *path, FileMode mode);

    File *FdOpen(int fd, FileMode mode);

    int GetFd(File *file);

    ArSSize Read(File *file, unsigned char *buf, ArSize count);

    ArSSize ReadLine(File *file, unsigned char **buf, ArSize buf_len);

    ArSize Tell(File *file);

    ArSSize Write(File *file, unsigned char *buf, ArSize count);

    ArSSize WriteObject(File *file, argon::object::ArObject *obj);

    void Close(File *file);

} // namespace argon::object::io

ENUMBITMASK_ENABLE(argon::object::io::FileMode);

#endif // !ARGON_OBJECT_IO_IO_H_
