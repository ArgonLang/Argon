// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <sys/stat.h>

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
        ret = ARGON_CALL_FUNC5(file_, read, func, self, args, 1);
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
        nullptr,
        nullptr,
        nullptr,
        -1
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
