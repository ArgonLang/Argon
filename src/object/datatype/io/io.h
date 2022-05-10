// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_IO_IO_H_
#define ARGON_OBJECT_IO_IO_H_

#include <cstring>
#include <mutex>

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

#define ARGON_OBJECT_IO_DEFAULT_BUFSIZE                 4096
#define ARGON_OBJECT_IO_DEFAULT_READLINE_BUFSIZE        6
#define ARGON_OBJECT_IO_DEFAULT_READLINE_BUFSIZE_INC    ARGON_OBJECT_IO_DEFAULT_READLINE_BUFSIZE

namespace argon::object::io {

    enum class FileMode : unsigned int {
        READ = 1u,
        WRITE = 1u << 1u,
        APPEND = 1u << 2u,

        // PRIVATE/NOT EXPORTED FIELDS
        _IS_TERM = 1u << 3u,
        _IS_PIPE = 1u << 4u
    };

    enum class FileBufferMode : unsigned int {
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
        std::mutex lock;

        struct {
            unsigned char *buf;
            unsigned char *cur;

            long cap;
            long len;
            long wlen;
            FileBufferMode mode;
        } buffer;

        FileMode mode;
        ArSize cur;
        int fd;
    };

    extern const argon::object::TypeInfo *type_file_;

    extern const argon::object::TypeInfo *type_readT_;
    extern const argon::object::TypeInfo *type_writeT_;

    struct BufferedIO : argon::object::ArObject {
        ArObject *base;

        ArObject *buffer;   // Bytes
        ArObject *blocksz;  // Integer

        ArSize index;
    };

    extern const argon::object::TypeInfo *type_buffered_reader_;
    extern const argon::object::TypeInfo *type_buffered_writer_;

    bool IOInit();

    bool Flush(File *file);

    bool Isatty(File *file);

    bool IsSeekable(File *file);

    bool Seek(File *file, ArSSize offset, FileWhence whence);

    bool SetBuffer(File *file, unsigned char *buf, ArSSize cap, FileBufferMode mode);

    File *Open(char *path, FileMode mode);

    File *FdOpen(int fd, FileMode mode);

    int Close(File *file);

    int GetFd(File *file);

    ArSSize Read(File *file, unsigned char *buf, ArSize count);

    ArSSize ReadLine(File *file, unsigned char **buf, ArSize buf_len);

    ArSize Tell(File *file);

    ArSSize Write(File *file, unsigned char *buf, ArSize count);

    inline ArSSize WriteString(File *file, const char *str) {
        return Write(file, (unsigned char *) str, strlen(str));
    }

    ArSSize WriteObject(File *file, argon::object::ArObject *obj);

    ArSSize WriteObjectStr(File *file, argon::object::ArObject *obj);

} // namespace argon::object::io

ENUMBITMASK_ENABLE(argon::object::io::FileMode);

#endif // !ARGON_OBJECT_IO_IO_H_
