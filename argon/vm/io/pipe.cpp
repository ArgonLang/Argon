// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#include <argon/vm/datatype/error.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <windows.h>

#else

#include <unistd.h>
#include <fcntl.h>

#endif


#include <argon/vm/io/pipe.h>

using namespace argon::vm::datatype;
using namespace argon::vm::io;

#ifdef _ARGON_PLATFORM_WINDOWS

bool argon::vm::io::MakePipe(IOHandle *read, IOHandle *write, int flags) {
    SECURITY_ATTRIBUTES secattr{};

    secattr.nLength = sizeof(SECURITY_ATTRIBUTES);
    secattr.lpSecurityDescriptor = nullptr;

    secattr.bInheritHandle = true;

    if (flags & O_CLOEXEC)
        secattr.bInheritHandle = false;

    if (CreatePipe(read, write, &secattr, 0) == 0) {
        ErrorFromWinErr();

        return false;
    }

    return true;
}

#else
bool argon::vm::io::MakePipe(IOHandle *read, IOHandle *write, int flags) {
    IOHandle pipefd[2]{};

    if (pipe(pipefd) < 0) {
        ErrorFromErrno(errno);

        return false;
    }

    if (fcntl(pipefd[0], F_SETFD, flags) < 0) {
        ErrorFromErrno(errno);

        close(pipefd[0]);
        close(pipefd[1]);

        return false;
    }

    if (fcntl(pipefd[1], F_SETFD, flags) < 0) {
        ErrorFromErrno(errno);

        close(pipefd[0]);
        close(pipefd[1]);

        return false;
    }

    *read = pipefd[0];
    *write = pipefd[1];

    return true;
}
#endif

void argon::vm::io::ClosePipe(IOHandle pipe) {
#ifdef _ARGON_PLATFORM_WINDOWS
    assert(false);
#else
    close(pipe);
#endif
}
