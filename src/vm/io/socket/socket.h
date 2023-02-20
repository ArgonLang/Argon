// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_IO_SOCKET_SOCKET_H_
#define ARGON_VM_IO_SOCKET_SOCKET_H_

namespace argon::vm::io::socket {
    constexpr const char *kGAIError[] = {
            (const char *) "GAIError",
    };

    void ErrorFromSocket();

} // namespace argon::vm::io::socket

#endif // !ARGON_VM_IO_SOCKET_SOCKET_H_
