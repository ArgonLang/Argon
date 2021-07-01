// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "bytesops.h"

using namespace argon::object;

void FillBadCharTable(int *table, const unsigned char *pattern, ArSSize len, bool reverse) {
    // Reset table
    for (int i = 0; i < 256; i++)
        table[i] = (int) len;

    // Fill table
    // value = len(pattern) - index - 1
    for (int i = 0; i < len; i++)
        table[pattern[i]] = reverse ? i : ((int) len) - i - 1;
}

ArSSize DoSearch(int *table, const unsigned char *buf, ArSSize blen, const unsigned char *pattern, ArSSize plen) {
    ArSSize cursor = plen - 1;
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
            return cursor - (plen - 1);
    }

    return -1;
}

ArSSize DoRSearch(int *table, const unsigned char *buf, ArSSize blen, const unsigned char *pattern, ArSSize plen) {
    ArSSize cursor = blen - plen;
    ArSSize i;

    FillBadCharTable(table, pattern, plen, true);

    while (cursor >= 0) {
        for (i = 0; i < plen; i++) {
            if (buf[cursor + i] != pattern[i]) {
                cursor = (cursor - i) - table[buf[cursor - i]];
                break;
            }
        }

        if (i == plen)
            return cursor;
    }

    return -1;
}

long argon::object::support::Count(const unsigned char *buf, ArSSize blen, const unsigned char *pattern,
                                   ArSSize plen, long n) {
    ArSSize counter = 0;
    ArSSize idx = 0;
    ArSSize lmatch;

    if (n == 0)
        return 0;

    while ((counter < n || n == -1) && (lmatch = Find(buf + idx, blen - idx, pattern, plen)) > -1) {
        counter++;
        idx += lmatch + plen;
    }

    return counter;
}

long argon::object::support::Find(const unsigned char *buf, ArSSize blen, const unsigned char *pattern,
                                  ArSSize plen, bool reverse) {
    /*
     * Implementation of Boyer-Moore-Horspool algorithm
     */

    int delta1[256] = {}; // Bad Character table

    if (((long) blen) < 0)
        return -2; // Too big

    if (plen > blen)
        return -1;

    if (reverse)
        return DoRSearch(delta1, buf, blen, pattern, plen);

    return DoSearch(delta1, buf, blen, pattern, plen);
}
