// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MODULES_IO_IO_H_
#define ARGON_MODULES_IO_IO_H_

#include <object/arobject.h>
#include <utils/enum_bitmask.h>

namespace argon::modules::io {

    enum class FileMode : unsigned char {
        READ = 1,
        WRITE = (unsigned char) 1 << (unsigned char) 1,
        APPEND = (unsigned char) 1 << (unsigned char) 2
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
        size_t cur;
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

    extern const argon::object::TypeInfo type_file_;

    bool Flush(File *file);

    bool Isatty(File *file);

    bool Seek(File *file, ssize_t offset, FileWhence whence);

    bool SetBuffer(File *file, unsigned char *buf, size_t cap, FileBufferMode mode);

    File *Open(char *path, FileMode mode);

    File *FdOpen(int fd, FileMode mode);

    int GetFd(File *file);

    ssize_t Read(File *file, unsigned char *buf, size_t count);

    ssize_t ReadLine(File *file, unsigned char *buf, size_t buf_len);

    size_t Tell(File *file);

    ssize_t Write(File *file, unsigned char *buf, size_t count);

    ssize_t WriteObject(File *file, argon::object::ArObject *obj);

    void Close(File *file);

} // namespace argon::modules::io

ENUMBITMASK_ENABLE(argon::modules::io::FileMode);

#endif // !ARGON_MODULES_IO_IO_H_
