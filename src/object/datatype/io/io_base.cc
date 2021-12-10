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