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
#include <object/datatype/integer.h>
#include <object/datatype/error.h>

#include "io.h"

using namespace argon::object;
using namespace argon::object::io;

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
    // TODO thread_lock
    if (file->buffer.mode == FileBufferMode::NONE || file->buffer.wlen == 0)
        return true;

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

bool argon::object::io::Isatty(File *file) {
    return ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM);
}

bool argon::object::io::IsSeekable(File *file) {
    return !(ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_TERM) ||
             ENUMBITMASK_ISTRUE(file->mode, FileMode::_IS_PIPE));
}

bool argon::object::io::Seek(File *file, ArSSize offset, FileWhence whence) {
    // TODO thread_lock
    if (!Flush(file))
        return false;

    return seek_wrap(file, offset, whence);
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

bool argon::object::io::SetBuffer(File *file, unsigned char *buf, ArSSize cap, FileBufferMode mode) {
    // TODO thread_lock
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
    file->buffer.cap = cap; // TODO: cap

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

        if (!SetBuffer(file, nullptr, 0, buf_mode)) {
            Release(file);
            return nullptr;
        }
    }

    return file;
#undef ISFIFO
}

int argon::object::io::Close(File *file) {
    // TODO thread_lock
    int err;

    if (file->fd < 0)
        return 0;

    if (file->buffer.mode != FileBufferMode::NONE)
        SetBuffer(file, nullptr, 0, FileBufferMode::NONE);

    while ((err = close(file->fd)) != 0 && errno == EINTR);

    file->fd = -1;

    if (err != 0)
        ErrorSetFromErrno();

    return err;
}

int argon::object::io::GetFd(File *file) {
    // TODO thread_lock
    Flush(file);
    return file->fd;
}

ArSSize FillBuffer(File *file) {
    ArSSize nbytes = -1;

    // check if buffer is empty
    if (file->buffer.cur < file->buffer.buf + file->buffer.len)
        return (file->buffer.buf + file->buffer.len) - file->buffer.cur;

    if (Flush(file)) {
        if ((nbytes = read_os_wrap(file, file->buffer.buf, file->buffer.cap)) > 0)
            file->buffer.len = nbytes;
    }

    return nbytes;
}

ArSSize ReadFromBuffer(File *file, unsigned char *buf, ArSize count) {
    ArSize to_read = (file->buffer.buf + file->buffer.len) - file->buffer.cur;
    ArSize nbytes = 0;

    while ((count > to_read) && to_read > 0) {
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

                if (rbytes == 0)
                    return (ArSSize) nbytes;

                nbytes += rbytes;
                count -= rbytes;
            } while (count >= file->buffer.cap);
        }

        if ((to_read = FillBuffer(file)) < 0)
            return -1;
    }

    if (to_read < count)
        count = to_read;

    argon::memory::MemoryCopy(buf + nbytes, file->buffer.cur, count);
    file->buffer.cur += count;
    nbytes += count;

    return (ArSSize) nbytes;
}

ArSSize argon::object::io::Read(File *file, unsigned char *buf, ArSize count) {
    // TODO thread_lock

    if (file->buffer.mode != FileBufferMode::NONE)
        return ReadFromBuffer(file, buf, count);

    return read_os_wrap(file, buf, count);
}

ArSSize ReadLineSeekable(File *file, unsigned char **buf, ArSize buf_len) {
    unsigned char *tmp_buf;
    unsigned char *cursor;
    unsigned char *nl_ptr;
    ArSize start_pos;
    ArSize total;
    ArSSize err;

    bool allocate = false;

    start_pos = Tell(file);

    // leave space for NULL
    buf_len--;

    total = 0;

    do {
        if (*buf == nullptr || allocate) {
            buf_len = allocate ? (buf_len + 1) + ARGON_OBJECT_IO_DEFAULT_READLINE_BUFSIZE_INC :
                      ARGON_OBJECT_IO_DEFAULT_READLINE_BUFSIZE;

            allocate = true;

            if ((tmp_buf = (unsigned char *) argon::memory::Realloc(*buf, buf_len)) == nullptr) {
                argon::vm::Panic(error_out_of_memory);
                Seek(file, (ArSSize) start_pos, FileWhence::START);
                goto ERROR;
            }

            *buf = tmp_buf;

            // leave space for NULL
            buf_len--;
        }

        cursor = *buf + total;

        if ((err = read_os_wrap(file, cursor, buf_len - total)) < 0)
            goto ERROR;

        if (err > 0) {
            total += err;

            if ((nl_ptr = (unsigned char *) argon::memory::MemoryFind(cursor, '\n', err)) != nullptr) {
                total = nl_ptr - *buf;

                if (!Seek(file, (ArSSize) (start_pos + total), FileWhence::START))
                    goto ERROR;

                break;
            }

            cursor += err;
        }
    } while (allocate && cursor - *buf == buf_len);

    *(*buf + total) = 0;
    total++;

    if (allocate) {
        if ((tmp_buf = (unsigned char *) argon::memory::Realloc(*buf, total)) == nullptr)
            goto ERROR;

        *buf = tmp_buf;
    }

    return (ArSSize) total;

    ERROR:
    if (allocate) {
        argon::memory::Free(*buf);
        *buf = nullptr;
    }

    return err >= 0 ? -1 : err;
}

ArSSize ReadLineSlow(File *file, unsigned char **buf, ArSize buf_len) {
    unsigned char *cursor = *buf;
    unsigned char *tmp_buf;
    ArSize total;
    ArSSize err;

    bool allocate = false;

    // leave space for NULL
    buf_len--;

    total = 0;

    while (buf_len != 0 || allocate && err > 0) {
        if (*buf == nullptr || allocate && buf_len == 0) {
            buf_len = allocate ? (total + 1) + ARGON_OBJECT_IO_DEFAULT_READLINE_BUFSIZE_INC :
                      ARGON_OBJECT_IO_DEFAULT_READLINE_BUFSIZE;

            allocate = true;

            if ((tmp_buf = (unsigned char *) argon::memory::Realloc(*buf, buf_len)) == nullptr) {
                argon::vm::Panic(error_out_of_memory);
                goto ERROR;
            }

            *buf = tmp_buf;
            cursor = tmp_buf + total;

            // leave space for NULL
            buf_len--;
        }

        if ((err = read_os_wrap(file, cursor, 1)) < 0)
            goto ERROR;

        if (err > 0) {
            buf_len--;
            total++;

            if (*cursor == '\n')
                break;

            cursor++;
        }
    }

    *(cursor++) = 0;

    if (allocate) {
        if ((tmp_buf = (unsigned char *) argon::memory::Realloc(*buf, total)) == nullptr)
            goto ERROR;

        *buf = tmp_buf;
    }

    return cursor - *buf;

    ERROR:
    if (allocate) {
        argon::memory::Free(*buf);
        *buf = nullptr;
    }

    return err >= 0 ? -1 : err;
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
        if(IsSeekable(file))
            return ReadLineSeekable(file, buf, buf_len);

        return ReadLineSlow(file, buf, buf_len);
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
    // TODO thread_lock

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

        if (Flush(file)) {
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
    // TODO thread_lock

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