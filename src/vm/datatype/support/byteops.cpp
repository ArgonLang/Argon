// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cctype>

#include "byteops.h"

using namespace argon::vm::datatype;
using namespace argon::vm::datatype::support;

void FillBadCharTable(int *table, const unsigned char *pattern, ArSize len, bool reverse) {
    // Reset table
    for (int i = 0; i < 256; i++)
        table[i] = (int) len;

    // Fill table
    // value = len(pattern) - index - 1
    for (int i = 0; i < len; i++)
        table[pattern[i]] = reverse ? i : ((int) len) - i - 1;
}

ArSSize DoSearch(int *table, const unsigned char *buf, ArSize blen, const unsigned char *pattern, ArSize plen) {
    auto cursor = (ArSSize) (plen - 1);
    ArSSize i;

    FillBadCharTable(table, pattern, plen, false);

    while (cursor < blen) {
        for (i = 0; i < plen; i++) {
            if (buf[cursor - i] != pattern[(plen - 1) - i]) {
                cursor = (cursor - i) + table[buf[cursor - i]];
                break;
            }
        }

        if (i == plen)
            return cursor - (ArSSize) (plen - 1);
    }

    return -1;
}

ArSSize DoRSearch(int *table, const unsigned char *buf, ArSize blen, const unsigned char *pattern, ArSize plen) {
    auto cursor = (ArSSize) (blen - plen);
    ArSSize i;

    FillBadCharTable(table, pattern, plen, true);

    while (cursor >= 0) {
        for (i = 0; i < plen; i++) {
            if (buf[cursor + i] != pattern[i]) {
                cursor = cursor - table[buf[cursor - i]];
                break;
            }
        }

        if (i == plen)
            return cursor;
    }

    return -1;
}

ArSSize argon::vm::datatype::support::Count(const unsigned char *buf, ArSize blen, const unsigned char *pattern,
                                            ArSize plen, long n) {
    ArSSize counter = 0;
    ArSSize idx = 0;
    ArSSize lmatch;

    if (n == 0)
        return 0;

    while (counter < n || n == -1) {
        lmatch = Find(buf + idx, blen - idx, pattern, plen);
        if (lmatch < 0) {
            break;
        }

        counter++;
        idx += (ArSSize) (lmatch + plen);
    }

    return counter;
}

ArSSize argon::vm::datatype::support::Find(const unsigned char *buf, ArSize blen, const unsigned char *pattern,
                                           ArSize plen, bool reverse) {
    /*
     * Implementation of Boyer-Moore-Horspool algorithm
     */

    int delta1[256] = {}; // Bad Character table

    if (plen > blen)
        return -1;

    if (reverse)
        return DoRSearch(delta1, buf, blen, pattern, plen);

    return DoSearch(delta1, buf, blen, pattern, plen);
}

ArSSize argon::vm::datatype::support::FindNewLine(const unsigned char *buf, ArSize *inout_len, bool universal) {
    ArSize index = 0;

    while (index < *inout_len) {
        if (universal && buf[index] == '\r') {
            if (index + 1 < *inout_len && buf[index + 1] == '\n') {
                *inout_len = index;
                return (ArSSize) index + 2;
            }

            *inout_len = index;
            return (ArSSize) index + 1;
        }

        if (buf[index] == '\n') {
            *inout_len = index;
            return (ArSSize) index + 1;
        }

        index++;
    }

    return -1;
}

long argon::vm::datatype::support::FindWhitespace(const unsigned char *buf, ArSize *inout_len, bool reverse) {
    ArSize idx = 0;
    ArSSize start;

    if (*inout_len == 0)
        return -1;

    if (reverse) {
        idx = *inout_len;

        while (idx > 0 && !std::isspace(buf[idx - 1]))
            idx--;

        *inout_len = idx--;

        while (idx > 0 && std::isspace(buf[idx - 1]))
            idx--;

        return (long) idx;
    }

    while (idx < *inout_len && !std::isspace(buf[idx]))
        idx++;

    start = (ArSSize) idx++;

    if (start == *inout_len)
        return -1;

    while (idx < *inout_len && std::isspace(buf[idx]))
        idx++;

    *inout_len = idx;

    return start;
}