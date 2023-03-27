// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SCANNER_IBUFFER_H_
#define ARGON_LANG_SCANNER_IBUFFER_H_

#include <cstdio>

namespace argon::lang::scanner {

    class InputBuffer {
        unsigned char *start = nullptr;
        unsigned char *cur = nullptr;
        unsigned char *inp = nullptr;
        unsigned char *end = nullptr;

        bool release = true;

    public:
        InputBuffer(const unsigned char *buffer, unsigned long length) : start((unsigned char *) buffer),
                                                                         cur((unsigned char *) buffer),
                                                                         inp((unsigned char *) buffer + length),
                                                                         end((unsigned char *) buffer + length),
                                                                         release(false) {}

        InputBuffer() = default;

        ~InputBuffer();

        bool AppendInput(const unsigned char *buffer, int length);

        int Peek(bool advance);

        int ReadFile(FILE *fd, int length);
    };

} // namespace argon::lang::scanner

#endif // !ARGON_LANG_SCANNER_IBUFFER_H_
