// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_IO_IO_H_
#define ARGON_OBJECT_IO_IO_H_

#include <cstring>
#include <mutex>

#include <utils/enum_bitmask.h>

#include <object/arobject.h>
#include <object/datatype/bytes.h>

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
#define ARGON_OBJECT_IO_BUFSIZE_INC     (ARGON_OBJECT_IO_DEFAULT_BUFSIZE / 2)

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
    extern const argon::object::TypeInfo *type_textinputT_;
    extern const argon::object::TypeInfo *type_textioT_;
    extern const argon::object::TypeInfo *type_writeT_;

    struct BufferedIO : argon::object::ArObject {
        std::mutex lock;

        ArObject *base;

        ArObject *buffer;   // Bytes
        ArObject *blocksz;  // Integer

        ArSize index;
    };

    extern const argon::object::TypeInfo *type_buffered_reader_;
    extern const argon::object::TypeInfo *type_buffered_writer_;

    ArSSize Read(File *file, unsigned char *buf, ArSize count);

    ArSSize ReadLine(File *file, unsigned char **buf, ArSSize buf_len);

    ArSize Tell(File *file);

    ArSSize Write(File *file, unsigned char *buf, ArSize count);

    ArSSize WriteObject(File *file, argon::object::ArObject *obj);

    ArSSize WriteObjectStr(File *file, argon::object::ArObject *obj);

    inline ArSSize WriteString(File *file, const char *str) {
        return Write(file, (unsigned char *) str, strlen(str));
    }

    bool Flush(File *file);

    bool IOInit();

    bool Isatty(File *file);

    bool IsSeekable(File *file);

    bool Seek(File *file, ArSSize offset, FileWhence whence);

    bool SetBuffer(File *file, unsigned char *buf, ArSSize cap, FileBufferMode mode);

    template<typename T, typename Func>
    Bytes *Read(Func func(T *, unsigned char *, ArSize), T *bio, ArSSize size) {
        unsigned char *buffer;
        Bytes *bytes;

        ArSize bufcap = size;
        ArSize index = 0;
        ArSSize rlen = 0;

        if (size < 0)
            bufcap = ARGON_OBJECT_IO_DEFAULT_BUFSIZE;
        else if (size == 0)
            return BytesNew(0, true, false, true);

        if ((buffer = ArObjectNewRaw<unsigned char *>(bufcap)) == nullptr)
            return nullptr;

        do {
            index += rlen;

            if (index == bufcap) {
                unsigned char *tmp = ArObjectRealloc(buffer, bufcap + ARGON_OBJECT_IO_BUFSIZE_INC);
                if (tmp == nullptr) {
                    argon::memory::Free(buffer);
                    return nullptr;
                }

                buffer = tmp;
                bufcap += ARGON_OBJECT_IO_BUFSIZE_INC;
            }

            if ((rlen = func(bio, buffer + index, bufcap - index)) < 0) {
                argon::memory::Free(buffer);
                return nullptr;
            }
        } while ((rlen >= bufcap - index) && size < 0);

        index += rlen;

        if ((bytes = BytesNewHoldBuffer(buffer, index, bufcap, true)) == nullptr)
            argon::memory::Free(buffer);

        return bytes;
    }

    int Close(File *file);

    int GetFd(File *file);

    File *FdOpen(int fd, FileMode mode);

    File *Open(char *path, FileMode mode);

} // namespace argon::object::io

ENUMBITMASK_ENABLE(argon::object::io::FileMode);

#endif // !ARGON_OBJECT_IO_IO_H_
