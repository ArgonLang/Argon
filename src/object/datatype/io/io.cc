// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <cerrno>

#include <utils/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS
#include <io.h>
#else

#include <unistd.h>

#endif

#include <vm/runtime.h>

#include <object/datatype/bool.h>
#include <object/datatype/bytes.h>
#include <object/datatype/nil.h>
#include <object/datatype/integer.h>
#include <object/datatype/error.h>

#include "io.h"

using namespace argon::object;
using namespace argon::object::io;

ARGON_METHOD5(file_, close, "Flush and close this file."
                            ""
                            "This method has no effect if the file is already closed."
                            ""
                            "- Returns: nil", 0, false) {
    auto *file = (File *) self;

    Close(file);

    if (argon::vm::IsPanicking())
        return nullptr;

    return IncRef(NilVal);
}

ARGON_METHOD5(file_, flush, "Flush the write buffers (if applicable)."
                            ""
                            "Does nothing for read-only and non-blocking stream."
                            ""
                            "- Returns: nil", 0, false) {
    auto *file = (File *) self;

    Flush(file);

    if (argon::vm::IsPanicking())
        return nullptr;

    return IncRef(NilVal);
}

ARGON_METHOD5(file_, getbufmode, "Returns the current buffer mode."
                                 ""
                                 "A buffer mode can be a one of this values:"
                                 "  * BUF_NONE"
                                 "  * BUF_LINE"
                                 "  * BUF_BLOCK"
                                 ""
                                 "- Returns: buffer mode (integer)."
                                 ""
                                 "# SEE"
                                 "- setbufmode: set buffering mode.", 0, false) {
    return IntegerNew((IntegerUnderlying) ((File *) self)->mode);
}

ARGON_METHOD5(file_, getfd, "Return the underlying file descriptor (integer)."
                            ""
                            "- Returns: integer.", 0, false) {
    int fd = GetFd(((File *) self));
    return IntegerNew(fd);
}

ARGON_METHOD5(file_, isatty, "Test whether a file descriptor refers to a terminal."
                             ""
                             "- Returns: true if this descriptor refers to a terminal, false otherwise.",
              0, false) {
    return BoolToArBool(Isatty((File *) self));
}

ARGON_METHOD5(file_, isclosed, "Test if this file is closed."
                               ""
                               "- Returns: true if file is closed, false otherwise.", 0, false) {
    return BoolToArBool(((File *) self)->fd < 0);
}

ARGON_METHOD5(file_, isseekable, "Test if the file is seekable."
                                 ""
                                 "- Returns: true if the file is seekable, false otherwise.", 0, false) {
    return BoolToArBool(IsSeekable((File *) self));
}

ARGON_FUNCTION5(file_, open, "Open file and return corresponding file object."
                             ""
                             "The operations that are allowed on the file and how these are performed are defined "
                             "by the mode parameter. The parameter mode value can be one or a combination of these:"
                             "  * O_READ"
                             "  * O_WRITE"
                             "  * O_APPEND"
                             ""
                             "- Parameters:"
                             "  - path: file path."
                             "  - mode: open mode."
                             "- Returns: file object.", 2, false) {
    File *file;
    char *path;

    FileMode flags;

    if (!AR_TYPEOF(argv[0], type_string_))
        return ErrorFormat(type_type_error_, "file::open expected path as string, not '%s'", AR_TYPE_NAME(argv[0]));

    if (!AR_TYPEOF(argv[1], type_integer_))
        return ErrorFormat(type_type_error_, "file::open expected mode as integer, not '%s'", AR_TYPE_NAME(argv[1]));

    path = (char *) ((String *) argv[0])->buffer;
    flags = (FileMode) ((Integer *) argv[1])->integer;

    if ((file = Open(path, flags)) == nullptr)
        return nullptr;

    return file;
}

ARGON_FUNCTION5(file_, openfd, "Create file object from file descriptor."
                               ""
                               "The operations that are allowed on the file and how these are performed are defined "
                               "by the mode parameter. The parameter mode value can be one or a combination of these:"
                               "  * O_READ"
                               "  * O_WRITE"
                               "  * O_APPEND"
                               ""
                               "- Parameters:"
                               "  - fd: file descriptor (integer)."
                               "  - mode: open mode."
                               "- Returns: file object.", 2, false) {
    File *file;

    int fd;
    FileMode flags;

    if (!AR_TYPEOF(argv[0], type_integer_))
        return ErrorFormat(type_type_error_, "file::openfd expected fd as integer, not '%s'", AR_TYPE_NAME(argv[0]));

    if (!AR_TYPEOF(argv[1], type_integer_))
        return ErrorFormat(type_type_error_, "file::open expected mode as integer, not '%s'", AR_TYPE_NAME(argv[1]));

    fd = (int) ((Integer *) argv[0])->integer;
    flags = (FileMode) ((Integer *) argv[1])->integer;

    if ((file = FdOpen(fd, flags)) == nullptr)
        return nullptr;

    return file;
}

ARGON_METHOD5(file_, read, "Read up to size bytes from the file and return them."
                           ""
                           "As a convenience, if size is unspecified or -1, all bytes until EOF are returned"
                           "(equivalent to to file::readall())."
                           "With size = -1, read() may be using multiple calls to the stream."
                           ""
                           "- Parameter size: number of bytes to read from the stream."
                           "- Returns: bytes object.", 1, false) {
    struct stat st{};

    unsigned char *buf = nullptr;
    unsigned char *tmp;
    File *file;
    Bytes *ret;

    ArSSize index = 0;
    ArSSize buflen = 0;
    ArSSize blksize;
    ArSSize rdlen;

    bool known_len = true;

    if (!AR_TYPEOF(*argv, type_integer_))
        return ErrorFormat(type_type_error_, "file::read expected integer as size, not '%s'", AR_TYPE_NAME(*argv));

    file = (File *) self;
    blksize = ((Integer *) *argv)->integer;

    if (blksize < 0) {
        if (fstat(file->fd, &st) < 0) {
            ErrorSetFromErrno();
            return nullptr;
        }

        blksize = st.st_size;
        if (ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM)) {
#if _ARGON_PLATFORM != windows
            blksize = st.st_blksize;
#else
            blksize = ARGON_OBJECT_IO_DEFAULT_BUFSIZE;
#endif
            known_len = false;
        }
    }

    if (blksize == 0)
        return BytesNew(0, true, true, true);

    do {
        if (buflen - index == 0) {
            if ((tmp = (unsigned char *) argon::memory::Realloc(buf, buflen + blksize)) == nullptr)
                goto error;

            buf = tmp;
            buflen += blksize;
        }

        if ((rdlen = Read(file, buf + index, buflen - index)) < 0)
            goto error;

        index += rdlen;
    } while (rdlen != 0 && !known_len);

    if (index == 0) {
        argon::memory::Free(buf);
        return BytesNew(0, true, true, true);
    }

    if ((ret = BytesNewHoldBuffer(buf, index, buflen, true)) == nullptr)
        goto error;

    return ret;

    error:
    argon::memory::Free(buf);
    if (IsSeekable(file))
        Seek(file, -index, FileWhence::CUR);
    return nullptr;
}

ARGON_METHOD5(file_, readall, "Read and return all the bytes from the stream until EOF."
                              ""
                              "May be using multiple calls to the stream."
                              "Equivalent to file:read(-1)."
                              ""
                              "- Returns: bytes object.", 0, false) {
    ArObject *args[] = {nullptr};
    ArObject *ret = nullptr;

    *args = IntegerNew(-1);

    if (*args != nullptr) {
        ret = ARGON_CALL_FUNC5(file_, read, origin, self, args, 1);
        Release(*args);
    }

    return ret;
}

ARGON_METHOD5(file_, readinto, "Read bytes into a pre-allocated, writable bytes-like object."
                               ""
                               "- Parameter obj: bytes-like writable object."
                               "- Returns: number of bytes read.", 1, false) {
    ArBuffer buffer = {};
    File *file;
    ArSSize rlen;

    file = (File *) self;

    if (!BufferGet(*argv, &buffer, ArBufferFlags::WRITE))
        return nullptr;

    rlen = Read(file, buffer.buffer, buffer.len);
    BufferRelease(&buffer);

    if (rlen < 0)
        return nullptr;

    return IntegerNew(rlen);
}

ARGON_METHOD5(file_, readline, "Read and return a single line from file."
                               ""
                               "- Returns: bytes object that contain a single line read.", 0, false) {
    auto *file = (File *) self;
    unsigned char *buf = nullptr;
    Bytes *bytes;

    ArSSize len;

    if ((len = ReadLine(file, &buf, 0)) < 0)
        return nullptr;

    if ((bytes = BytesNewHoldBuffer(buf, len, len, true)) == nullptr) {
        argon::memory::Free(buf);

        if (IsSeekable(file))
            Seek(file, -len, FileWhence::CUR);
    }

    return bytes;
}

ARGON_METHOD5(file_, setbufmode, "Set buffering mode."
                                 ""
                                 "A buffer mode can be a one of this values:"
                                 "  * BUF_NONE"
                                 "  * BUF_LINE"
                                 "  * BUF_BLOCK"
                                 ""
                                 "- Parameter mode: buffer mode (integer)."
                                 "- Returns: nil"
                                 ""
                                 "# SEE"
                                 "- getbufmode: returns the current buffer mode.", 1, false) {
    auto *file = (File *) self;
    IntegerUnderlying mode;

    if (!AR_TYPEOF(*argv, type_integer_))
        return ErrorFormat(type_type_error_, "file::setbufmode expected integer as mode, not '%s'",
                           AR_TYPE_NAME(*argv));

    mode = ((Integer *) *argv)->integer;

    if (mode != (IntegerUnderlying) FileBufferMode::NONE &&
        mode != (IntegerUnderlying) FileBufferMode::BLOCK &&
        mode != (IntegerUnderlying) FileBufferMode::LINE)
        return ErrorFormat(type_value_error_, "file::setbufmode invalid value (%d)", mode);

    SetBuffer(file, nullptr, 0, (FileBufferMode) mode);

    if (argon::vm::IsPanicking())
        return nullptr;

    return IncRef(NilVal);
}

ARGON_METHOD5(file_, seek, "Change the stream position to the given byte offset."
                           ""
                           "Offset is interpreted relative to the position indicated by whence."
                           "Whence can be one of this value:"
                           "    * SEEK_START"
                           "    * SEEK_CUR"
                           "    * SEEK_END"
                           ""
                           "- Parameters:"
                           "    - offset: offset in byte."
                           "    - whence: whence parameter (integer)."
                           "- Returns: nil", 2, false) {
    auto *file = (File *) self;
    IntegerUnderlying mode;

    if (!AR_TYPEOF(argv[0], type_integer_))
        return ErrorFormat(type_type_error_, "file::seek expected integer as offset, not '%s'",
                           AR_TYPE_NAME(argv[0]));

    if (!AR_TYPEOF(argv[1], type_integer_))
        return ErrorFormat(type_type_error_, "file::seek expected integer as whence, not '%s'",
                           AR_TYPE_NAME(argv[1]));

    mode = ((Integer *) argv[1])->integer;

    if (mode != (IntegerUnderlying) FileWhence::START &&
        mode != (IntegerUnderlying) FileWhence::CUR &&
        mode != (IntegerUnderlying) FileWhence::END)
        return ErrorFormat(type_value_error_, "file::seek invalid whence value (%d)", mode);

    Seek(file, ((Integer *) argv[0])->integer, (FileWhence) mode);

    if (argon::vm::IsPanicking())
        return nullptr;

    return IncRef(NilVal);
}

ARGON_METHOD5(file_, tell, "Return the current stream position."
                           ""
                           "- Returns: current stream position (integer).", 0, false) {
    return IntegerNew(Tell((File *) self));
}

ARGON_METHOD5(file_, write, "Write a bytes-like object to underlying stream."
                            ""
                            "- Parameter obj: bytes-like object to write to."
                            "- Returns: bytes written (integer).", 1, false) {
    auto *file = (File *) self;
    ArSSize wlen;

    wlen = WriteObject(file, *argv);

    if (wlen < 0)
        return nullptr;

    return IntegerNew(wlen);
}

const NativeFunc file_methods[] = {
        file_close_,
        file_flush_,
        file_getbufmode_,
        file_getfd_,
        file_isatty_,
        file_isclosed_,
        file_isseekable_,
        file_open_,
        file_openfd_,
        file_read_,
        file_readall_,
        file_readinto_,
        file_readline_,
        file_setbufmode_,
        file_seek_,
        file_tell_,
        file_write_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots file_obj = {
        file_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

bool file_istrue(File *self) {
    return self->fd > -1;
}

ArObject *file_compare(File *self, ArObject *other, CompareMode mode) {
    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self != other)
        return BoolToArBool(self->fd == ((File *) other)->fd);

    return BoolToArBool(true);
}

void file_cleanup(File *self) {
    Close(self);
}

ArObject *file_str(File *self) {
    char mode[24] = {};
    const char *buffered;

    int idx = 0;

    if (ENUMBITMASK_ISTRUE(self->mode, FileMode::READ)) {
        memcpy(mode, "O_READ", 6);
        idx += 6;
    }

    if (ENUMBITMASK_ISTRUE(self->mode, FileMode::WRITE)) {
        memcpy(mode + idx, "|", 1);
        idx += 1;
        memcpy(mode + idx, "O_WRITE", 7);
        idx += 7;
    }

    if (ENUMBITMASK_ISTRUE(self->mode, FileMode::APPEND)) {
        memcpy(mode + idx, "|", 1);
        idx += 1;
        memcpy(mode + idx, "O_APPEND", 8);
        idx += 8;
    }

    mode[idx] = '\0';

    if (self->buffer.mode == FileBufferMode::NONE)
        buffered = "NONE";
    else if (self->buffer.mode == FileBufferMode::LINE)
        buffered = "LINE";
    else
        buffered = "BLOCK";

    return StringNewFormat("<file fd: %d, mode: %s, buffered: %s>", self->fd, mode, buffered);
}

const TypeInfo FileType = {
        TYPEINFO_STATIC_INIT,
        "file",
        nullptr,
        sizeof(File),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) file_cleanup,
        nullptr,
        (CompareOp) file_compare,
        (BoolUnaryOp) file_istrue,
        nullptr,
        (UnaryOp) file_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &file_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::object::io::type_file_ = &FileType;

ArSSize read_os_wrap(File *file, void *buf, ArSize nbytes) {
    ArSSize r = read(file->fd, buf, nbytes);

    if (r >= 0) {
        file->cur += r;
        return r;
    }

    ErrorSetFromErrno();

    return r;
}

ArSSize write_os_wrap(File *file, const void *buf, ArSize n) {
    ArSSize written = write(file->fd, buf, n);

    if (written >= 0) {
        file->cur += written;
        return written;
    }

    ErrorSetFromErrno();

    return written;
}

bool seek_wrap(File *file, ArSSize offset, FileWhence whence) {
    ArSSize pos;
    int _whence;

    switch (whence) {
        case FileWhence::START:
            _whence = SEEK_SET;
            break;
        case FileWhence::CUR:
            _whence = SEEK_CUR;
            break;
        case FileWhence::END:
            _whence = SEEK_END;
            break;
    }

    if ((pos = lseek(file->fd, offset, _whence)) >= 0) {
        file->cur = pos;
        return true;
    }

    ErrorSetFromErrno();
    return false;
}

bool argon::object::io::Flush(File *file) {
    if (file->buffer.mode == FileBufferMode::NONE || file->buffer.wlen == 0)
        return true;

    if (ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM) ||
        ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_PIPE) ||
        seek_wrap(file, file->cur - file->buffer.len, FileWhence::START)) {
        if (write_os_wrap(file, file->buffer.buf, file->buffer.wlen) >= 0) {
            file->buffer.cur = file->buffer.buf;
            file->buffer.len = 0;
            file->buffer.wlen = 0;
            return true;
        }
    }

    return false;
}

bool argon::object::io::Isatty(File *file) {
    return ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM);
}

bool argon::object::io::IsSeekable(File *file) {
    return !(ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM) ||
             ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_PIPE));
}

bool argon::object::io::Seek(File *file, ArSSize offset, FileWhence whence) {
    if (file->buffer.mode != FileBufferMode::NONE && file->buffer.wlen > 0) {
        if (!Flush(file))
            return false;
    }

    if (seek_wrap(file, offset, whence)) {
        if (file->buffer.mode != FileBufferMode::NONE) {
            file->buffer.cur = file->buffer.buf;
            file->buffer.len = 0;
            file->buffer.wlen = 0;
            return true;
        }
    }

    return false;
}

inline ArSSize FindBestBufSize(File *file) {
#if _ARGON_PLATFORM != windows
    struct stat st{};

    if (ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM))
        return ARGON_OBJECT_IO_DEFAULT_BUFSIZE;

    if (fstat(file->fd, &st) >= 0)
        return st.st_blksize > 8192 ? 8192 : st.st_blksize;
#endif

    return ARGON_OBJECT_IO_DEFAULT_BUFSIZE;
}

bool argon::object::io::SetBuffer(File *file, unsigned char *buf, ArSSize cap, FileBufferMode mode) {
    bool ok = true;

    Flush(file);

    // Remove old buffer (if any)
    if (file->buffer.buf != nullptr)
        argon::memory::Free(file->buffer.buf);

    if (mode != FileBufferMode::NONE) {
        if (cap == 0) {
            buf = nullptr;
            cap = FindBestBufSize(file);
        }

        if (buf == nullptr) {
            if ((buf = (unsigned char *) argon::memory::Alloc(cap)) == nullptr) {
                mode = FileBufferMode::NONE;
                cap = 0;
                ok = false;
                argon::vm::Panic(error_out_of_memory);
            }
        }
    } else {
        buf = nullptr;
        cap = 0;
    }

    file->buffer.mode = mode;
    file->buffer.buf = buf;
    file->buffer.cur = buf;
    file->buffer.cap = cap;

    return ok;
}

File *argon::object::io::Open(char *path, FileMode mode) {
    unsigned int omode = O_RDONLY;
    int fd;

    File *file;

    if (ENUMBITMASK_ISTRUE(mode, FileMode::WRITE)) {
        omode |= (unsigned int) O_WRONLY | (unsigned int) O_CREAT;

        if (ENUMBITMASK_ISTRUE(mode, FileMode::READ))
            omode = (unsigned int) O_RDWR | (unsigned int) O_CREAT;
    }

    if (ENUMBITMASK_ISTRUE(mode, FileMode::APPEND))
        omode |= (unsigned int) O_APPEND;

    if ((fd = open(path, (int) omode)) < 0) {
        return (File *) ErrorSetFromErrno();
    }

    if ((file = FdOpen(fd, mode)) == nullptr)
        close(fd);

    return file;
}

File *argon::object::io::FdOpen(int fd, FileMode mode) {
#ifdef S_ISFIFO
#define ISFIFO(st)  S_ISFIFO((st).st_mode)
#else
#ifdef S_IFIFO
#define ISFIFO(st)  ((st).st_mode & S_IFMT) == S_IFIFO
#elif defined _S_IFIFO  // Windows
#define ISFIFO(st)  ((st).st_mode & S_IFMT) == _S_IFIFO
#endif
#endif
    struct stat st{};
    File *file;

    FileBufferMode buf_mode;

    file = ArObjectNew<File>(RCType::INLINE, type_file_);

    if (file != nullptr) {
        file->fd = fd;
        file->mode = mode;
        file->cur = 0;

        file->buffer.buf = nullptr;
        file->buffer.cur = nullptr;

        file->buffer.cap = 0;
        file->buffer.len = 0;
        file->buffer.wlen = 0;

        if (isatty(fd) != 0) {
            file->mode |= FileMode::_IS_TERM;
            buf_mode = FileBufferMode::LINE;
        } else {
            if (fstat(file->fd, &st) < 0) {
                Release(file);
                return nullptr;
            }

            if (ISFIFO(st))
                file->mode |= FileMode::_IS_PIPE;

            buf_mode = FileBufferMode::BLOCK;
        }

        if (!SetBuffer(file, nullptr, 0, buf_mode)) {
            Release(file);
            return nullptr;
        }
    }

    return file;
#undef ISFIFO
}

int argon::object::io::GetFd(File *file) {
    Flush(file);
    return file->fd;
}

ArSSize FillBuffer(File *file) {
    ArSSize nbytes = -1;

    // check if buffer is empty
    if (file->buffer.cur < file->buffer.buf + file->buffer.len)
        return (file->buffer.buf + file->buffer.len) - file->buffer.cur;

    if (Flush(file)) {
        file->buffer.len = 0;
        file->buffer.cur = file->buffer.buf;

        if ((nbytes = read_os_wrap(file, file->buffer.buf, file->buffer.cap)) > 0)
            file->buffer.len = nbytes;
    }

    return nbytes;
}

ArSSize ReadFromBuffer(File *file, unsigned char *buf, ArSize count) {
    ArSize to_read = (file->buffer.buf + file->buffer.len) - file->buffer.cur;
    ArSize nbytes = 0;

    while (count > to_read) {
        argon::memory::MemoryCopy(buf + nbytes, file->buffer.cur, to_read);
        file->buffer.cur += to_read;
        nbytes += to_read;
        count -= to_read;

        if (count >= file->buffer.cap) {
            ArSSize rbytes;

            if (!Flush(file))
                return -1;

            do {
                if ((rbytes = read_os_wrap(file, buf + nbytes, file->buffer.cap)) < 0)
                    return -1;
                else if (rbytes == 0)
                    return nbytes;

                nbytes += rbytes;
                count -= rbytes;
            } while (count >= file->buffer.cap);
        }

        if (FillBuffer(file) < 0)
            return -1;

        // EOF
        if ((to_read = file->buffer.len) == 0)
            count = 0;
    }

    if (file->buffer.len < count)
        count = file->buffer.len;

    argon::memory::MemoryCopy(buf + nbytes, file->buffer.cur, count);
    file->buffer.cur += count;
    nbytes += count;

    return nbytes;
}

ArSSize argon::object::io::Read(File *file, unsigned char *buf, ArSize count) {
    if (file->buffer.mode != FileBufferMode::NONE)
        return ReadFromBuffer(file, buf, count);

    return read_os_wrap(file, buf, count);
}

ArSSize argon::object::io::ReadLine(File *file, unsigned char **buf, ArSize buf_len) {
    unsigned char *line = *buf;
    unsigned char *idx;
    ArSize allocated = 1;
    ArSize n = 0;
    ArSize len;

    bool found = false;

    if (*buf != nullptr && buf_len == 0)
        return 0;

    if (file->buffer.mode == FileBufferMode::NONE) {
        ErrorFormat(type_io_error_, "file::readline unsupported in unbuffered mode");
        return -1;
    }

    while (n < buf_len - 1 && !found) {
        if (FillBuffer(file) < 0)
            goto error;

        if ((len = file->buffer.buf + file->buffer.len - file->buffer.cur) == 0)
            break;

        if (buf_len > 0 && len > (buf_len - 1) - n)
            len = (buf_len - 1) - n;

        if ((idx = (unsigned char *) argon::memory::MemoryFind(file->buffer.cur, '\n', len)) != nullptr) {
            len = idx - file->buffer.cur;
            found = true;
        }

        if (*buf == nullptr) {
            allocated += n + len;
            if ((idx = (unsigned char *) argon::memory::Realloc(line, allocated)) == nullptr) {
                argon::vm::Panic(error_out_of_memory);
                goto error;
            }
            line = idx;
        }

        argon::memory::MemoryCopy(line + n, file->buffer.cur, len);
        n += len;

        if (found)
            len++;

        file->buffer.cur += len;
    }

    if (line != nullptr)
        line[n] = '\0';

    if (*buf == nullptr)
        *buf = line;

    return n;

    error:
    if (*buf == nullptr)
        memory::Free(line);

    return -1;
}

ArSize argon::object::io::Tell(File *file) {
    if (file->buffer.mode == FileBufferMode::NONE)
        return file->cur;

    return (file->cur - file->buffer.len) + (file->buffer.cur - file->buffer.buf);
}

ArSSize WriteToBuffer(File *file, const unsigned char *buf, ArSize count) {
    unsigned char *cap_ptr = file->buffer.buf + file->buffer.cap;
    unsigned char *cur_start = file->buffer.cur;
    long wlen_start = file->buffer.wlen;
    ArSize writes = 0;

    while (writes < count) {
        if (file->buffer.cur < cap_ptr) {
            *file->buffer.cur++ = buf[writes++];

            file->buffer.wlen = file->buffer.cur - file->buffer.buf;

            if (file->buffer.mode == FileBufferMode::LINE && buf[writes - 1] == '\n')
                goto FLUSH;

            continue;
        }

        FLUSH:
        if (Flush(file)) {
            // store new start position of buffer cur and wlen
            cur_start = file->buffer.cur;
            wlen_start = 0;
        } else {
            // restore buffer
            file->buffer.cur = cur_start;
            file->buffer.wlen = wlen_start;
            return -1;
        }
    }

    return writes;
}

ArSSize argon::object::io::Write(File *file, unsigned char *buf, ArSize count) {
    if (file->buffer.mode != FileBufferMode::NONE)
        return WriteToBuffer(file, buf, count);

    return write_os_wrap(file, buf, count);
}

ArSSize argon::object::io::WriteObject(File *file, ArObject *obj) {
    ArBuffer buffer = {};

    if (!BufferGet(obj, &buffer, ArBufferFlags::READ))
        return -1;

    ArSSize nbytes = Write(file, buffer.buffer, buffer.len);
    BufferRelease(&buffer);
    return nbytes;
}

void argon::object::io::Close(File *file) {
    int err;

    if (file->fd < 0)
        return;

    if (file->buffer.mode != FileBufferMode::NONE)
        SetBuffer(file, nullptr, 0, FileBufferMode::NONE);

    // TODO: improve this
    do {
        err = close(file->fd);
    } while (err != 0 && errno == EINTR);

    if (err != 0)
        ErrorSetFromErrno();
    // EOL

    file->fd = -1;
}
