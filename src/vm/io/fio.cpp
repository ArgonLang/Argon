// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/datatype/arstring.h>
#include <vm/datatype/boolean.h>
#include <vm/datatype/bytes.h>
#include <vm/datatype/error.h>
#include <vm/datatype/function.h>
#include <vm/datatype/integer.h>
#include <vm/datatype/nil.h>

#include "fio.h"

#ifdef _ARGON_PLATFORM_WINDOWS

#include <io.h>
#include <Windows.h>

#undef ERROR
#else

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#endif

using namespace argon::vm::datatype;
using namespace argon::vm::io;

ARGON_METHOD(file_close, close,
             "Close this stream.\n"
             "\n"
             "This method has no effect if the stream is already closed.\n",
             nullptr, false, false) {
    if (!FileClose((File *) _self))
        return nullptr;

    return (ArObject *) IncRef(Nil);
}

ARGON_METHOD(file_getfd, getfd,
             "Return the underlying file descriptor.\n"
             "\n"
             "- Returns: File descriptor (UInt).\n",
             nullptr, false, false) {
    auto fd = GetFd((File *) _self);
    return (ArObject *) UIntNew(fd);
}

ARGON_METHOD(file_isatty, isatty,
             "Test whether a file descriptor refers to a terminal.\n"
             "\n"
             "- Returns: True if this descriptor refers to a terminal, false otherwise.\n",
             nullptr, false, false) {
    return BoolToArBool(Isatty((File *) _self));
}

ARGON_METHOD(file_isclosed, isclosed,
             "Test if this file is closed.\n"
             "\n"
             "- Returns: True if file is closed, false otherwise.\n",
             nullptr, false, false) {
    auto self = (File *) _self;
    bool closed;

#ifdef _ARGON_PLATFORM_WINDOWS
    closed = self->handle == INVALID_HANDLE_VALUE;
#else
    close = self->handle < 0;
#endif

    return BoolToArBool(closed);
}

ARGON_METHOD(file_isseekable, isseekable,
             "Test if the file is seekable.\n"
             "\n"
             "- Returns: True if this file is seekable, false otherwise.\n",
             nullptr, false, false) {
    return BoolToArBool(IsSeekable((File *) _self));
}

// Inherited from Reader trait
ARGON_METHOD_INHERITED(file_read, read) {
    auto *self = (File *) _self;
    ArSize blksize = ((Integer *) *args)->sint;
    Bytes *ret;

    bool known_len = true;

    if (((Integer *) *args)->sint < 0) {
        if (!Isatty(self)) {
            if (!GetFileSize(self, &blksize)) {
                return nullptr;
            }
        } else {
            known_len = false;
            blksize = 4096;
        }
    }

    if (blksize == 0)
        return (ArObject *) BytesNew(0, true, false, true);

    unsigned char *buf = nullptr;
    unsigned char *tmp;
    ArSize index = 0;
    ArSize buflen = 0;
    ArSSize rdlen;

    do {
        if (buflen - index == 0) {
            if ((tmp = (unsigned char *) argon::vm::memory::Realloc(buf, buflen + blksize)) == nullptr)
                goto ERROR;

            buf = tmp;
            buflen += blksize;
        }

        if ((rdlen = Read(self, buf + index, buflen - index)) < 0)
            goto ERROR;

        index += rdlen;

    } while (rdlen != 0 && !known_len);

    if ((ret = BytesNewHoldBuffer(buf, buflen, index, true)) == nullptr)
        goto ERROR;

    return (ArObject *) ret;

    ERROR:
    argon::vm::memory::Free(buf);
    if (IsSeekable(self))
        Seek(self, -((ArSSize) index), FileWhence::CUR);

    return nullptr;
}

// Inherited from Reader trait
ARGON_METHOD_INHERITED(file_readinto, readinto) {
    ArBuffer buffer{};
    auto *self = (File *) _self;
    auto offset = ((Integer *) args[1])->sint;
    ArSSize rlen;

    if (!BufferGet(*args, &buffer, BufferFlags::WRITE))
        return nullptr;

    if (offset < 0)
        offset = 0;

    if (offset >= buffer.length) {
        ErrorFormat(kOverflowError[0], kOverflowError[2], AR_TYPE_NAME(*args), buffer.length, offset);
        BufferRelease(&buffer);
        return nullptr;
    }

    if ((rlen = Read(self, buffer.buffer + offset, buffer.length - offset)) < 0) {
        BufferRelease(&buffer);
        return nullptr;
    }

    BufferRelease(&buffer);

    return (ArObject *) IntNew(rlen);
}

ARGON_METHOD(file_seek, seek,
             "Change the file position to the given byte offset.\n"
             "\n"
             "Offset is interpreted relative to the position indicated by whence.\n"
             "Whence can be one of this value:\n"
             "    * SEEK_START\n"
             "    * SEEK_CUR\n"
             "    * SEEK_END\n"
             "\n"
             "- Parameters:\n"
             "    - offset: Offset in byte.\n"
             "    - whence: Whence parameter (Int).\n",
             "i: offset, i: whence", false, false) {
    IntegerUnderlying mode;

    mode = ((Integer *) args[1])->sint;

    if (mode != (IntegerUnderlying) FileWhence::START &&
        mode != (IntegerUnderlying) FileWhence::CUR &&
        mode != (IntegerUnderlying) FileWhence::END) {
        ErrorFormat(kValueError[0], "%s invalid whence value (%d)",
                    ARGON_RAW_STRING(((Function *) _func)->qname), mode);

        return nullptr;
    }

    if (!Seek((File *) _self, ((Integer *) *args)->sint, (FileWhence) mode))
        return nullptr;

    return (ArObject *) IncRef(Nil);
}

ARGON_METHOD(file_tell, tell,
             "Return the current file position.\n"
             "\n"
             "- Returns: Current file position.\n",
             nullptr, false, false) {
    ArSize pos;

    if (!Tell((File *) _self, &pos))
        return nullptr;

    return (ArObject *) UIntNew(pos);
}

// Inherited from Writer trait
ARGON_METHOD_INHERITED(file_write, write) {
    auto *self = (File *) _self;
    ArSSize written;

    if ((written = WriteObject(self, *args)) < 0)
        return nullptr;

    return (ArObject *) IntNew(written);
}

ARGON_METHOD(file_writestr, writestr,
             "Writes an object to the underlying stream.\n"
             "\n"
             "The behavior varies based on the object passed as a parameter. "
             "If the object supports the buffer protocol then it will be written directly into the underlying stream. "
             "Otherwise it will be converted to String and then written to the underlying stream.\n"
             "\n"
             "- Parameter obj: Object to write to.\n"
             "- Returns: Bytes written.\n",
             ": obj", false, false) {
    auto *self = (File *) _self;
    ArSSize written;

    if ((written = WriteObjectStr(self, *args)) < 0)
        return nullptr;

    return (ArObject *) IntNew(written);
}

const FunctionDef file_methods[] = {
        file_close,
        file_getfd,
        file_isatty,
        file_isclosed,
        file_isseekable,
        file_read,
        file_readinto,
        file_seek,
        file_tell,
        file_write,
        file_writestr,
        ARGON_METHOD_SENTINEL
};

TypeInfo *file_bases[] = {
        (TypeInfo *) type_reader_t_,
        (TypeInfo *) type_writer_t_,
        nullptr
};

const ObjectSlots file_objslot = {
        file_methods,
        nullptr,
        file_bases,
        nullptr,
        nullptr,
        -1
};

ArObject *file_compare(const File *self, ArObject *other, CompareMode mode) {
    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if ((ArObject *) self != other)
        return BoolToArBool(self->handle == ((File *) other)->handle);

    return BoolToArBool(true);
}

ArObject *file_repr(const File *self) {
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

#ifdef _ARGON_PLATFORM_WINDOWS
    return (ArObject *) StringFormat("<file fd: %x, mode: %s>", self->handle, mode);
#else
    return (ArObject *) StringFormat("<file fd: %d, mode: %s>", self->handle, mode);
#endif
}

bool file_dtor(File *self) {
#ifdef _ARGON_PLATFORM_WINDOWS
    if (self->handle != INVALID_HANDLE_VALUE)
        return CloseHandle(self->handle) != 0;
#else
    if (self->handle >= 0)
        return close(self->handle) == 0;
#endif

    return true;
}

TypeInfo FileType = {
        AROBJ_HEAD_INIT_TYPE,
        "File",
        nullptr,
        nullptr,
        sizeof(File),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) file_dtor,
        nullptr,
        nullptr,
        nullptr,
        (CompareOp) file_compare,
        (UnaryConstOp) file_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &file_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::io::type_file_ = &FileType;

// Platform Independent

ArSize argon::vm::io::GetFd(File *file) {
    return (ArSize) file->handle;
}

ArSSize argon::vm::io::WriteObject(File *file, datatype::ArObject *object) {
    ArBuffer buffer{};

    if (!BufferGet(object, &buffer, datatype::BufferFlags::READ))
        return -1;

    auto written = Write(file, buffer.buffer, buffer.length);

    BufferRelease(&buffer);

    return written;
}

ArSSize argon::vm::io::WriteObjectStr(File *file, datatype::ArObject *object) {
    auto *to_buf = IncRef(object);

    if (!IsBufferable(to_buf)) {
        ArObject *str;

        if ((str = Str(object)) == nullptr) {
            Release(to_buf);
            return -1;
        }

        Release(to_buf);
        to_buf = str;
    }

    auto written = WriteObject(file, to_buf);

    Release(to_buf);

    return written;
}

// ***

#ifdef _ARGON_PLATFORM_WINDOWS

ArSSize argon::vm::io::Read(File *file, unsigned char *buf, datatype::ArSize count) {
    DWORD read;

    if (!ReadFile(file->handle,
                  buf,
                  (DWORD) count,
                  &read,
                  nullptr)) {
        ErrorFromWinErr();
        return -1;
    }

    return (ArSSize) read;
}

ArSSize argon::vm::io::Write(File *file, const unsigned char *buf, datatype::ArSize count) {
    DWORD written;

    if (!WriteFile(file->handle,
                   buf,
                   (DWORD) count,
                   &written,
                   nullptr)) {
        ErrorFromWinErr();
        return -1;
    }

    return (ArSSize) written;
}

bool argon::vm::io::FileClose(File *file) {
    if (file->handle == INVALID_HANDLE_VALUE)
        return true;

    if (CloseHandle(file->handle) != 0) {
        file->handle = INVALID_HANDLE_VALUE;
        return true;
    }

    ErrorFromWinErr();
    return false;
}

bool argon::vm::io::GetFileSize(const File *file, datatype::ArSize *out_size) {
    BY_HANDLE_FILE_INFORMATION finfo{};

    if (!GetFileInformationByHandle(file->handle, &finfo)) {
        ErrorFromWinErr();
        return false;
    }

    *out_size = finfo.nFileSizeLow;

    return true;
}

bool argon::vm::io::Isatty(const File *file) {
    auto type = GetFileType(file->handle);
    return type == FILE_TYPE_CHAR;
}

bool argon::vm::io::IsSeekable(const File *file) {
    auto type = GetFileType(file->handle);
    return type != FILE_TYPE_CHAR && type != FILE_TYPE_PIPE;
}

bool argon::vm::io::Seek(const File *file, datatype::ArSSize offset, FileWhence whence) {
    int _whence;

    switch (whence) {
        case FileWhence::START:
            _whence = FILE_BEGIN;
            break;
        case FileWhence::CUR:
            _whence = FILE_CURRENT;
            break;
        case FileWhence::END:
            _whence = FILE_END;
            break;
    }

    if (SetFilePointer(file->handle,
                       offset,
                       nullptr,
                       _whence) == INVALID_SET_FILE_POINTER) {
        ErrorFromWinErr();
        return false;
    }

    return true;
}

bool argon::vm::io::Tell(const File *file, ArSize *out_pos) {
    *out_pos = SetFilePointer(file->handle, 0, nullptr, FILE_CURRENT);
    if (*out_pos == INVALID_SET_FILE_POINTER) {
        ErrorFromWinErr();
        return false;
    }

    return true;
}

File *argon::vm::io::FileNew(const char *path, FileMode mode) {
    DWORD omode = GENERIC_READ;
    DWORD cmode = OPEN_EXISTING;

    if (ENUMBITMASK_ISTRUE(mode, FileMode::WRITE)) {
        omode |= GENERIC_WRITE;

        if (ENUMBITMASK_ISTRUE(mode, FileMode::READ))
            omode = GENERIC_READ;

        cmode |= CREATE_NEW;
    }

    if (ENUMBITMASK_ISTRUE(mode, FileMode::APPEND))
        omode |= FILE_APPEND_DATA;

    auto handle = CreateFile(
            path,
            omode,
            0,
            nullptr,
            cmode,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        ErrorFromWinErr();
        return nullptr;
    }

    auto *file = MakeObject<File>(&FileType);
    if (file == nullptr) {
        CloseHandle(handle);
        return nullptr;
    }

    file->handle = handle;
    file->mode = mode;

    return file;
}

File *argon::vm::io::FileNew(int fd, FileMode mode) {
    auto handle = (HANDLE) _get_osfhandle(fd);
    if (handle == INVALID_HANDLE_VALUE) {
        ErrorFromErrno(errno);
        return nullptr;
    }

    auto *file = MakeObject<File>(&FileType);
    if (file == nullptr)
        return nullptr;

    file->handle = handle;
    file->mode = mode;

    return file;
}

#else

ArSSize argon::vm::io::Read(File *file, unsigned char *buf, datatype::ArSize count) {
    ArSSize rd;

    if ((rd = read(file->handle, buf, count)) < 0) {
        ErrorFromErrno(errno);
        return -1;
    }

    return rd;
}

ArSSize argon::vm::io::Write(File *file, const unsigned char *buf, datatype::ArSize count) {
    ArSSize written;

    if ((written = write(file->handle, buf, count)) < 0) {
        ErrorFromErrno(errno);
        return -1;
    }

    return written;
}

bool argon::vm::io::FileClose(File *file) {
    if (file->handle == -1)
        return true;

    if (close(file->handle) == 0) {
        file->handle = -1;
        return true;
    }

    ErrorFromErrno(errno);
    return false;
}

bool argon::vm::io::GetFileSize(const File *file, datatype::ArSize *out_size) {
    struct stat st{};

    if (Isatty(file))
        return false;

    if (fstat(file->handle, &st) >= 0) {
        *out_size = st.st_size;
        return true;
    }

    return false;
}

bool argon::vm::io::Isatty(const File *file) {
    return isatty(file->handle) != 0;
}

bool argon::vm::io::IsSeekable(const File *file) {
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

    if (fstat(file->handle, &st) < 0)
        return false;

    return !ISFIFO(st) && isatty(file->handle) == 0;
}

bool argon::vm::io::Seek(const File *file, datatype::ArSSize offset, FileWhence whence) {
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

    auto pos = lseek(file->handle, offset, _whence);
    if (pos < 0) {
        ErrorFromErrno(errno);
        return false;
    }

    return true;
}

bool argon::vm::io::Tell(const File *file, ArSize *out_pos) {
    auto pos = lseek(file->handle, 0, SEEK_CUR);
    if (pos < 0) {
        ErrorFromErrno(errno);
        return false;
    }

    *out_pos = pos;

    return true;
}

File *argon::vm::io::FileNew(const char *path, FileMode mode) {
    unsigned int omode = O_RDONLY;
    int fd;

    if (ENUMBITMASK_ISTRUE(mode, FileMode::WRITE)) {
        omode |= (unsigned int) O_WRONLY | (unsigned int) O_CREAT;

        if (ENUMBITMASK_ISTRUE(mode, FileMode::READ))
            omode = (unsigned int) O_RDWR | (unsigned int) O_CREAT;
    }

    if (ENUMBITMASK_ISTRUE(mode, FileMode::APPEND))
        omode |= (unsigned int) O_APPEND;

    if ((fd = open(path, (int) omode)) < 0) {
        ErrorFromErrno(errno);
        return nullptr;
    }

    auto *file = MakeObject<File>(&FileType);
    if (file == nullptr) {
        close(fd);
        return nullptr;
    }

    file->handle = fd;
    file->mode = mode;

    return file;
}

File *argon::vm::io::FileNew(int fd, FileMode mode) {
    errno = 0;
    if (fcntl(fd, F_GETFD) == -1) {
        ErrorFromErrno(errno);
        return nullptr;
    }

    auto *file = MakeObject<File>(&FileType);
    if (file == nullptr)
        return nullptr;

    file->handle = fd;
    file->mode = mode;

    return file;
}

#endif
