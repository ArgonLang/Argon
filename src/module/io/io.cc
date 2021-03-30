// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <cerrno>

#include <vm/runtime.h>

#include <object/datatype/bool.h>
#include <object/datatype/error.h>

#include "io.h"

using namespace argon::object;
using namespace argon::module::io;

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

bool file_equal(File *self, ArObject *other) {
    if (self != other) {
        if (self->type != other->type)
            return false;
        return self->fd == ((File *) other)->fd;
    }
    return true;
}

void file_cleanup(File *self) {
    Close(self);
}

const TypeInfo argon::module::io::type_file_ = {
        TYPEINFO_STATIC_INIT,
        "file",
        nullptr,
        sizeof(File),
        nullptr,
        (VoidUnaryOp) file_cleanup,
        nullptr,
        (CompareOp) file_compare,
        (BoolBinOp) file_equal,
        (BoolUnaryOp) file_istrue,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

ssize_t read_os_wrap(File *file, void *buf, size_t nbytes) {
    ssize_t r = read(file->fd, buf, nbytes);

    if (r > 0) {
        file->cur += r;
        return r;
    }

    ErrorFormat(ErrorFromErrno(), "[Errno %d] %s: fileno: %d", errno, strerror(errno), file->fd);

    return r;
}

ssize_t write_os_wrap(File *file, const void *buf, size_t n) {
    ssize_t written = write(file->fd, buf, n);

    if (written > 0) {
        file->cur += written;
        return written;
    }

    ErrorFormat(ErrorFromErrno(), "[Errno %d] %s: fileno: %d", errno, strerror(errno), file->fd);

    return written;
}

bool argon::module::io::Flush(File *file) {
    if (file->buffer.mode == FileBufferMode::NONE || file->buffer.wlen == 0)
        return true;

    if (ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM) ||
        ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_PIPE) ||
        Seek(file, file->cur - file->buffer.len, FileWhence::START)) {
        if (write_os_wrap(file, file->buffer.buf, file->buffer.wlen) >= 0) {
            file->buffer.cur = file->buffer.buf;
            file->buffer.len = 0;
            file->buffer.wlen = 0;
            return true;
        }
    }

    return false;
}

bool argon::module::io::Isatty(File *file) {
    return ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM);
}

bool argon::module::io::IsSeekable(File *file) {
    return !(ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM) ||
             ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_PIPE));
}

bool argon::module::io::Seek(File *file, ssize_t offset, FileWhence whence) {
    ssize_t pos;
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

    ErrorFormat(ErrorFromErrno(), "[Errno %d] %s: fileno: %d", errno, strerror(errno), file->fd);
    return false;
}

size_t FindBestBufSize(File *file) {
    struct stat st{};

    if (ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM))
        return 4096;

    if (fstat(file->fd, &st) < 0)
        return 4096;

    return st.st_blksize > 8192 ? 8192 : st.st_blksize;
}

bool argon::module::io::SetBuffer(File *file, unsigned char *buf, size_t cap, FileBufferMode mode) {
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
                argon::vm::Panic(OutOfMemoryError);
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

File *argon::module::io::Open(char *path, FileMode mode) {
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
        ErrorFormat(ErrorFromErrno(), "[Errno %d] %s: %s", EACCES, strerror(EACCES), path);
        return nullptr;
    }

    if ((file = FdOpen(fd, mode)) == nullptr)
        close(fd);

    return file;
}

File *argon::module::io::FdOpen(int fd, FileMode mode) {
    auto file = ArObjectNew<File>(RCType::INLINE, &type_file_);

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
            if (!SetBuffer(file, nullptr, 0, FileBufferMode::LINE)) {
                Release(file);
                return nullptr;
            }
        } else {
            struct stat st{};

            if (fstat(file->fd, &st) < 0) {
                Release(file);
                return nullptr;
            }

            if (S_ISFIFO(st.st_mode))
                file->mode |= FileMode::_IS_PIPE;
        }
    }

    return file;
}

int argon::module::io::GetFd(File *file) {
    Flush(file);
    return file->fd;
}

ssize_t FillBuffer(File *file) {
    ssize_t nbytes = -1;

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

ssize_t ReadFromBuffer(File *file, unsigned char *buf, size_t count) {
    size_t to_read = (file->buffer.buf + file->buffer.len) - file->buffer.cur;
    size_t nbytes = 0;

    while (count > to_read) {
        argon::memory::MemoryCopy(buf + nbytes, file->buffer.cur, to_read);
        file->buffer.cur += to_read;
        nbytes += to_read;
        count -= to_read;

        if (count >= file->buffer.cap) {
            ssize_t rbytes;

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

    argon::memory::MemoryCopy(buf + nbytes, file->buffer.cur, count);
    file->buffer.cur += count;
    nbytes += count;

    return nbytes;
}

ssize_t argon::module::io::Read(File *file, unsigned char *buf, size_t count) {
    if (file->buffer.mode != FileBufferMode::NONE)
        return ReadFromBuffer(file, buf, count);

    return read_os_wrap(file, buf, count);
}

ssize_t argon::module::io::ReadLine(File *file, unsigned char **buf, size_t buf_len) {
    unsigned char *line = *buf;
    unsigned char *idx;
    size_t allocated = 1;
    size_t n = 0;
    size_t len;

    bool found = false;

    if (*buf != nullptr && buf_len == 0)
        return 0;

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
                argon::vm::Panic(OutOfMemoryError);
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

size_t argon::module::io::Tell(File *file) {
    if (file->buffer.mode == FileBufferMode::NONE)
        return file->cur;

    return (file->cur - file->buffer.len) + (file->buffer.cur - file->buffer.buf);
}

ssize_t WriteToBuffer(File *file, const unsigned char *buf, size_t count) {
    unsigned char *cap_ptr = file->buffer.buf + file->buffer.cap;
    unsigned char *cur_start = file->buffer.cur;
    long wlen_start = file->buffer.wlen;
    size_t writes = 0;

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

ssize_t argon::module::io::Write(File *file, unsigned char *buf, size_t count) {
    if (file->buffer.mode != FileBufferMode::NONE)
        return WriteToBuffer(file, buf, count);

    return write_os_wrap(file, buf, count);
}

ssize_t argon::module::io::WriteObject(File *file, ArObject *obj) {
    ArBuffer buffer = {};

    if (!IsBufferable(obj) || !BufferGet(obj, &buffer, ArBufferFlags::READ))
        return -1;

    ssize_t nbytes = Write(file, buffer.buffer, buffer.len);
    BufferRelease(&buffer);
    return nbytes;
}

void argon::module::io::Close(File *file) {
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
        ErrorFormat(ErrorFromErrno(), "[Errno %d] %s: fileno: %d", errno, strerror(errno), file->fd);
    // EOL

    file->fd = -1;
}
