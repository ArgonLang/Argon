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

#include <Windows.h>

#undef ERROR

#endif

using namespace argon::vm::datatype;
using namespace argon::vm::io;

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

ARGON_METHOD(file_isseekable, isseekable,
             "Test if the file is seekable.\n"
             "\n"
             "- Returns: True if this file is seekable, false otherwise.\n",
             nullptr, false, false) {
    return BoolToArBool(IsSeekable((File *) _self));
}

ARGON_METHOD(file_read, read,
             "Read up to size bytes from the file and return them.\n"
             "\n"
             "As a convenience, if size is -1, all bytes until EOF are returned.\n"
             "With size = -1, read() may be using multiple calls to the stream.\n"
             "\n"
             "- Parameter size: Number of bytes to read from the file.\n"
             "- Returns: Bytes object.\n",
             "i: size", false, false) {
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

ARGON_METHOD(file_readinto, readinto,
             "Read bytes into a pre-allocated, writable bytes-like object.\n"
             "\n"
             "- Parameter obj: Bytes-like writable object.\n"
             "- Returns: Number of bytes read.\n",
             ": obj", false, false) {
    ArBuffer buffer{};
    auto *self = (File *) _self;
    ArSSize rlen;

    if (!BufferGet(*args, &buffer, BufferFlags::WRITE))
        return nullptr;

    if ((rlen = Read(self, buffer.buffer, buffer.length)) < 0) {
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

ARGON_METHOD(file_write, write,
             "Write a bytes-like object to underlying stream.\n"
             "\n"
             "- Parameter obj: Bytes-like object to write to.\n"
             "- Returns: Bytes written.\n",
             ": obj", false, false) {
    auto *self = (File *) _self;
    ArSSize written;

    if ((written = WriteObject(self, *args)) < 0)
        return nullptr;

    return (ArObject *) IntNew(written);
}

const FunctionDef file_methods[] = {
        file_getfd,
        file_isatty,
        file_isseekable,
        file_read,
        file_readinto,
        file_seek,
        file_tell,
        file_write,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots file_objslot = {
        file_methods,
        nullptr,
        nullptr,
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
    CloseHandle(self->handle);
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

    bool ok = ReadFile(file->handle,
                       buf,
                       (DWORD) count,
                       &read,
                       nullptr);

    if (!ok) {
        assert(false); // TODO
        return -1;
    }

    return (ArSSize) read;
}

ArSSize argon::vm::io::Write(File *file, const unsigned char *buf, datatype::ArSize count) {
    DWORD written;

    bool ok = WriteFile(file->handle,
                        buf,
                        (DWORD) count,
                        &written,
                        nullptr);

    if (!ok) {
        assert(false); // TODO
        return -1;
    }

    return (ArSSize) written;
}

bool argon::vm::io::GetFileSize(File *file, datatype::ArSize *out_size) {
    BY_HANDLE_FILE_INFORMATION finfo{};

    if (!GetFileInformationByHandle(file->handle, &finfo)) {
        assert(false); // TODO
        return false;
    }

    *out_size = finfo.nFileSizeLow;

    return true;
}

bool argon::vm::io::Isatty(File *file) {
    auto type = GetFileType(file->handle);
    return type == FILE_TYPE_CHAR;
}

bool argon::vm::io::IsSeekable(File *file) {
    auto type = GetFileType(file->handle);
    return type != FILE_TYPE_CHAR && type != FILE_TYPE_PIPE;
}

bool argon::vm::io::Seek(File *file, datatype::ArSSize offset, FileWhence whence) {
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

    auto pos = SetFilePointer(file->handle,
                              offset,
                              nullptr,
                              _whence);
    if (pos == INVALID_SET_FILE_POINTER) {
        assert(false); // TODO
        return false;
    }

    return true;
}

bool argon::vm::io::Tell(File *file, ArSize *out_pos) {
    *out_pos = SetFilePointer(file->handle, 0, nullptr, FILE_CURRENT);
    if (*out_pos == INVALID_SET_FILE_POINTER) {
        assert(false); // TODO
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
        assert(false); // TODO
    }

    auto *file = MakeObject<File>(&FileType);
    if (file == nullptr) {
        CloseHandle(handle);
        return nullptr;
    }

    file->handle = handle;
    file->mode = mode;

    ArObject *args[1]{};

    args[0] = (ArObject *) IntNew(-1);

    file_read_fn(nullptr, (ArObject *) file, args, nullptr, 1);

    return file;
}

#endif
