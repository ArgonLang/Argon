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
#include <object/datatype/error.h>
#include <object/datatype/integer.h>
#include <object/datatype/nil.h>

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
    int fd = GetFd((File *) self);
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

ARGON_METHOD5(file_, read, "Read up to size bytes from the file and return them."
                           ""
                           "As a convenience, if size is unspecified or -1, all bytes until EOF are returned"
                           "(equivalent to to file::readall())."
                           "With size = -1, read() may be using multiple calls to the stream."
                           ""
                           "- Parameter size: number of bytes to read from the stream."
                           "- Returns: (bytes, err)", 1, false) {
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

    if (!CheckArgs("i:size", func, argv, count))
        return nullptr;

    file = (File *) self;
    blksize = ((Integer *) *argv)->integer;

    if (blksize < 0) {
        if (fstat(file->fd, &st) < 0)
            ARGON_OBJECT_TUPLE_ERROR(ErrorNewFromErrno());

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
        return ARGON_OBJECT_TUPLE_SUCCESS(BytesNew(0, true, true, true));

    do {
        if (buflen - index == 0) {
            if ((tmp = ArObjectRealloc<unsigned char *>(buf, buflen + blksize)) == nullptr)
                goto ERROR;

            buf = tmp;
            buflen += blksize;
        }

        if ((rdlen = Read(file, buf + index, buflen - index)) < 0)
            goto ERROR;

        index += rdlen;
    } while (rdlen != 0 && !known_len);

    if (index == 0) {
        argon::memory::Free(buf);
        return ARGON_OBJECT_TUPLE_SUCCESS(BytesNew(0, true, true, true));
    }

    if ((ret = BytesNewHoldBuffer(buf, index, buflen, true)) == nullptr)
        goto ERROR;

    return ARGON_OBJECT_TUPLE_SUCCESS(ret);

    ERROR:
    argon::memory::Free(buf);
    if (IsSeekable(file))
        Seek(file, -index, FileWhence::CUR);
    return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());
}

ARGON_METHOD5(file_, readall, "Read and return all the bytes from the stream until EOF."
                              ""
                              "May be using multiple calls to the stream."
                              "Equivalent to file:read(-1)."
                              ""
                              "- Returns: (bytes, err)", 0, false) {
    ArObject *args[] = {nullptr};
    ArObject *ret = nullptr;

    *args = IntegerNew(-1);

    if (*args != nullptr) {
        ret = ARGON_CALL_FUNC5(file_, read, func, self, args, 1);
        Release(*args);
    }

    return ret;
}

ARGON_METHOD5(file_, readinto, "Read bytes into a pre-allocated, writable bytes-like object."
                               ""
                               "- Parameter obj: bytes-like writable object."
                               "- Returns: (number of bytes read, err)", 1, false) {
    ArBuffer buffer = {};
    File *file;
    ArSSize rlen;

    file = (File *) self;

    if (!BufferGet(*argv, &buffer, ArBufferFlags::WRITE))
        return nullptr;

    rlen = Read(file, buffer.buffer, buffer.len);
    BufferRelease(&buffer);

    if (rlen < 0)
        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());

    return ARGON_OBJECT_TUPLE_SUCCESS(IntegerNew(rlen));
}

ARGON_METHOD5(file_, readline, "Read and return a single line from file."
                               ""
                               "- Parameter size: maximum number of bytes to read from the stream."
                               "- Returns: (bytes, err)", 1, false) {
    auto *file = (File *) self;
    const auto *arint = (Integer *) argv[0];
    unsigned char *buffer;
    Bytes *bytes;
    ArSSize cap;
    ArSSize len;

    if (!CheckArgs("i:size", func, argv, count))
        return nullptr;

    cap = arint->integer;

    if (arint->integer == 0)
        return ARGON_OBJECT_TUPLE_SUCCESS(BytesNew(0, true, false, true));

    if (arint->integer < 0)
        cap = ARGON_OBJECT_IO_DEFAULT_BUFSIZE;

    if ((buffer = ArObjectNewRaw<unsigned char *>(cap)) == nullptr)
        return nullptr;

    if ((len = ReadLine(file, &buffer, cap)) < 0) {
        argon::memory::Free(buffer);
        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());
    }

    if ((bytes = BytesNewHoldBuffer(buffer, len, len, true)) == nullptr) {
        argon::memory::Free(buffer);

        if (IsSeekable(file))
            Seek(file, -len, FileWhence::CUR);

        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());
    }

    return ARGON_OBJECT_TUPLE_SUCCESS(bytes);
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
                            "- Returns: (bytes written, err)", 1, false) {
    auto *file = (File *) self;
    ArSSize wlen;

    if ((wlen = WriteObject(file, *argv)) < 0)
        return ARGON_OBJECT_TUPLE_ERROR(argon::vm::GetLastNonFatalError());

    return ARGON_OBJECT_TUPLE_SUCCESS(IntegerNew(wlen));
}

const NativeFunc file_methods[] = {
        file_close_,
        file_flush_,
        file_getbufmode_,
        file_getfd_,
        file_isatty_,
        file_isclosed_,
        file_isseekable_,
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

const TypeInfo *file_bases[] = {
        type_readT_,
        type_writeT_,
        nullptr
};

const ObjectSlots file_obj = {
        file_methods,
        nullptr,
        file_bases,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

bool file_istrue(const File *self) {
    return self->fd > -1;
}

ArObject *file_compare(const File *self, ArObject *other, CompareMode mode) {
    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self != other)
        return BoolToArBool(self->fd == ((File *) other)->fd);

    return BoolToArBool(true);
}

void file_cleanup(File *self) {
    Close(self);
    self->lock.~mutex();
}

ArObject *file_str(const File *self) {
    char mode[24] = {};
    const char *buffered;

    int idx = 0;

    if (ENUMBITMASK_ISTRUE(self->mode, FileMode::READ)) {
        memcpy(mode, "O_READ", 6);
        idx += 6;
    }

    if (ENUMBITMASK_ISTRUE(self->mode, FileMode::WRITE)) {
        if (idx != 0) {
            memcpy(mode + idx, "|", 1);
            idx += 1;
        }
        memcpy(mode + idx, "O_WRITE", 7);
        idx += 7;
    }

    if (ENUMBITMASK_ISTRUE(self->mode, FileMode::APPEND)) {
        if (idx != 0) {
            memcpy(mode + idx, "|", 1);
            idx += 1;
        }
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
        "File",
        nullptr,
        sizeof(File),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) file_cleanup,
        nullptr,
        (CompareOp) file_compare,
        (BoolUnaryOp) file_istrue,
        nullptr,
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

// *** FILE IO_BASE ***

ArSSize read_os_wrap(File *file, void *buf, ArSize nbytes) {
    ArSSize r = read(file->fd, buf, nbytes);

    if (r >= 0)
        file->cur += r;
    else
        ErrorSetFromErrno();

    return r;
}

ArSSize write_os_wrap(File *file, const void *buf, ArSize n) {
    ArSSize written = write(file->fd, buf, n);

    if (written >= 0)
        file->cur += written;
    else
        ErrorSetFromErrno();

    return written;
}

bool argon::object::io::IOInit() {
#define INIT_TYPE(type)                         \
    if(!TypeInit((TypeInfo*)(type), nullptr))   \
        return false

    INIT_TYPE(io::type_file_);
    INIT_TYPE(io::type_buffered_reader_);
    INIT_TYPE(io::type_buffered_writer_);
    INIT_TYPE(io::type_readT_);
    INIT_TYPE(io::type_writeT_);

    return true;
#undef INIT_TYPE
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

bool FlushNB(File *file) {
    if (file->buffer.mode == FileBufferMode::NONE)
        return true;

    if (file->buffer.wlen == 0) {
        file->buffer.cur = file->buffer.buf;
        return true;
    }

    if (ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM) ||
        ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_PIPE) ||
        seek_wrap(file, (ArSSize) (file->cur - file->buffer.len), FileWhence::START)) {
        if (write_os_wrap(file, file->buffer.buf, file->buffer.wlen) >= 0) {
            file->buffer.cur = file->buffer.buf;
            file->buffer.len = 0;
            file->buffer.wlen = 0;
            return true;
        }
    }

    return false;
}

bool argon::object::io::Flush(File *file) {
    std::unique_lock lock(file->lock);

    return FlushNB(file);
}

bool argon::object::io::Isatty(File *file) {
    return ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM);
}

bool argon::object::io::IsSeekable(File *file) {
    return !(ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM) ||
             ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_PIPE));
}

bool SeekNB(File *file, ArSSize offset, FileWhence whence) {
    if (!FlushNB(file))
        return false;

    return seek_wrap(file, offset, whence);
}

bool argon::object::io::Seek(File *file, ArSSize offset, FileWhence whence) {
    std::unique_lock lock(file->lock);

    return SeekNB(file, offset, whence);
}

inline ArSSize FindBestBufSize(File *file) {
#ifndef _ARGON_PLATFORM_WINDOWS
    struct stat st{};

    if (ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM))
        return ARGON_OBJECT_IO_DEFAULT_BUFSIZE;

    if (fstat(file->fd, &st) >= 0)
        return st.st_blksize > 8192 ? 8192 : st.st_blksize;
#endif

    return ARGON_OBJECT_IO_DEFAULT_BUFSIZE;
}

bool SetBufferNB(File *file, unsigned char *buf, ArSSize cap, FileBufferMode mode) {
    bool ok = true;

    FlushNB(file);

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
    file->buffer.cap = cap; // TODO: cap

    return ok;
}

bool argon::object::io::SetBuffer(File *file, unsigned char *buf, ArSSize cap, FileBufferMode mode) {
    std::unique_lock lock(file->lock);

    return SetBufferNB(file, buf, cap, mode);
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

    if ((fd = open(path, (int) omode)) < 0)
        return (File *) ErrorSetFromErrno();

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

    FileBufferMode buf_mode;
    File *file;

    if ((file = ArObjectNew<File>(RCType::INLINE, type_file_)) != nullptr) {
        file->fd = fd;
        file->cur = 0;
        file->mode = mode;

        argon::memory::MemoryZero(&file->buffer, sizeof(File::buffer));

        if (isatty(fd) != 0) {
            file->mode |= FileMode::_IS_TERM;
            buf_mode = FileBufferMode::LINE;
        } else {
            if (fstat(file->fd, &st) < 0) {
                Release(file);
                return (File *) ErrorSetFromErrno();
            }

            if (ISFIFO(st))
                file->mode |= FileMode::_IS_PIPE;

            buf_mode = FileBufferMode::BLOCK;
        }

        if (!SetBufferNB(file, nullptr, 0, buf_mode)) {
            Release(file);
            return nullptr;
        }

        new(&file->lock) std::mutex();
    }

    return file;
#undef ISFIFO
}

int argon::object::io::Close(File *file) {
    std::unique_lock lock(file->lock);

    int err;

    if (file->fd < 0)
        return 0;

    if (file->buffer.mode != FileBufferMode::NONE)
        SetBufferNB(file, nullptr, 0, FileBufferMode::NONE);

    while ((err = close(file->fd)) != 0 && errno == EINTR);

    file->fd = -1;

    if (err != 0)
        ErrorSetFromErrno();

    return err;
}

int argon::object::io::GetFd(File *file) {
    std::unique_lock lock(file->lock);

    FlushNB(file);
    return file->fd;
}

ArSSize FillBuffer(File *file) {
    ArSSize nbytes = -1;

    // check if buffer is empty
    if (file->buffer.cur < file->buffer.buf + file->buffer.len)
        return (file->buffer.buf + file->buffer.len) - file->buffer.cur;

    if (FlushNB(file)) {
        file->buffer.len = 0;

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

            if (!FlushNB(file))
                return -1;

            do {
                if ((rbytes = read_os_wrap(file, buf + nbytes, file->buffer.cap)) < 0)
                    return -1;

                if (rbytes == 0)
                    return (ArSSize) nbytes;

                nbytes += rbytes;
                count -= rbytes;
            } while (count >= file->buffer.cap);
        }

        if ((to_read = FillBuffer(file)) < 0)
            return -1;

        // Check EOF
        if (to_read == 0)
            count = 0;
    }

    if (to_read < count)
        count = to_read;

    argon::memory::MemoryCopy(buf + nbytes, file->buffer.cur, count);
    file->buffer.cur += count;
    nbytes += count;

    return (ArSSize) nbytes;
}

ArSSize argon::object::io::Read(File *file, unsigned char *buf, ArSize count) {
    std::unique_lock lock(file->lock);

    if (file->buffer.mode != FileBufferMode::NONE)
        return ReadFromBuffer(file, buf, count);

    return read_os_wrap(file, buf, count);
}

ArSSize argon::object::io::ReadLine(File *file, unsigned char **buf, ArSSize buf_len) {
    unsigned char *line = *buf;
    unsigned char *tmp;
    ArSize allocated = 1;
    ArSize total = 0;
    ArSize rlen;
    ArSSize next;

    bool checknl = false;

    if (file->buffer.mode == FileBufferMode::NONE) {
        ErrorFormat(type_io_error_, "file::readline unsupported in unbuffered mode, try using BufferedReader");
        return -1;
    }

    if (buf_len == 0 || (buf_len < 0 && *buf != nullptr))
        return 0;

    while (buf_len > 0 || *buf == nullptr) {
        if (FillBuffer(file) < 0) {
            if (*buf == nullptr)
                memory::Free(line);
            return -1;
        }

        rlen = (file->buffer.buf + file->buffer.len) - file->buffer.cur;

        if (checknl) {
            if (*file->buffer.cur == '\n')
                file->buffer.cur++;
            break;
        }

        if (rlen == 0)
            return (ArSSize) total;

        if (buf_len > 0 && rlen > buf_len)
            rlen = buf_len;

        next = argon::object::support::FindNewLine(file->buffer.cur, &rlen);

        if (*buf == nullptr) {
            allocated += total + rlen;

            if (next > 0)
                allocated++;

            if ((tmp = ArObjectRealloc<unsigned char *>(line, allocated)) == nullptr) {
                if (*buf == nullptr)
                    memory::Free(line);
                return -1;
            }
            line = tmp;
        }

        if (next > 0) {
            argon::memory::MemoryCopy(line + total, file->buffer.cur, rlen);
            *(line + total + rlen) = '\n';
            file->buffer.cur += next;
            total += rlen + 1;

            if (*(file->buffer.cur - 1) == '\r') {
                // Check again in case of \r\total at the edge of buffer
                checknl = true;
                continue;
            }

            break;
        }

        argon::memory::MemoryCopy(line + total, file->buffer.cur, rlen);
        file->buffer.cur += rlen;
        buf_len -= (ArSSize) rlen;
        total += rlen;
    }

    if (*buf == nullptr)
        *buf = line;

    return (ArSSize) total;
}

ArSize argon::object::io::Tell(File *file) {
    std::unique_lock lock(file->lock);

    if (file->buffer.mode == FileBufferMode::NONE)
        return file->cur;

    return (file->cur - file->buffer.len) + (file->buffer.cur - file->buffer.buf);
}

ArSSize WriteToBuffer(File *file, const unsigned char *buf, ArSize count) {
    unsigned char *cap_ptr = file->buffer.buf + file->buffer.cap;
    unsigned char *cur_start = file->buffer.cur;
    long wlen_start = file->buffer.wlen;
    ArSSize writes = 0;

    while (writes < count) {
        if (file->buffer.cur < cap_ptr) {
            *file->buffer.cur++ = buf[writes++];

            file->buffer.wlen++;

            if (file->buffer.mode != FileBufferMode::LINE || buf[writes - 1] != '\n')
                continue;
        }

        if (FlushNB(file)) {
            // store new start position of buffer cur and wlen
            cur_start = file->buffer.cur;
            wlen_start = file->buffer.wlen;
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
    std::unique_lock lock(file->lock);

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

ArSSize argon::object::io::WriteObjectStr(File *file, ArObject *obj) {
    ArBuffer buffer = {};
    ArObject *to_buf;
    ArSSize nbytes = -1;

    to_buf = IncRef(obj);

    if (!IsBufferable(to_buf)) {
        String *str;

        if ((str = (String *) ToString(obj)) == nullptr)
            goto ERROR;

        Release(to_buf);
        to_buf = str;
    }

    if (!BufferGet(to_buf, &buffer, ArBufferFlags::READ))
        goto ERROR;

    nbytes = Write(file, buffer.buffer, buffer.len);
    BufferRelease(&buffer);

    ERROR:
    Release(to_buf);
    return nbytes;
}