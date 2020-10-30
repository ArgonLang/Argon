// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "bytesops.h"

long argon::object::support::Count(const unsigned char *buf, size_t blen, const unsigned char *pattern, size_t plen,
                                   long n) {
    long counter = 0;
    size_t idx = 0;

    if (n == 0)
        return 0;

    long lmatch;
    while ((counter < n || n == -1) && (lmatch = Find(buf + idx, blen - idx, pattern, plen)) > -1) {
        counter++;
        idx += lmatch + plen;
    }

    return counter;
}

long argon::object::support::Find(const unsigned char *buf, size_t blen, const unsigned char *pattern, size_t plen,
                                  bool reverse) {
    /*
     * Implementation of Boyer-Moore-Horspool algorithm
     */

    size_t delta1[256] = {}; // Bad Character table
    size_t cursor = plen - 1;
    bool ok = false;

    if (((long) blen) < 0)
        return -2; // Too big

    if (plen > blen)
        return -1;

    // Fill delta1 table
    for (size_t &i:delta1)
        i = plen;

    for (size_t i = 0; i < plen; i++)
        delta1[pattern[i]] = plen - i - 1;

    // Do search
    if (!reverse) {
        while (cursor < blen && !ok) {
            for (size_t i = 0; i < plen; i++) {
                if (pattern[(plen - 1) - i] != buf[cursor - i]) {
                    cursor = (cursor - i) + delta1[buf[cursor - i]];
                    ok = false;
                    break;
                }
                ok = true;
            }
        }
        cursor--;
    } else {
        cursor = (blen - 1) - (plen - 1);

        while (cursor >= 0 && !ok) {
            for (size_t i = 0; i < plen; i++) {
                if (pattern[i] != buf[cursor + i]) {
                    if (delta1[buf[cursor + i]] > cursor + i)
                        goto not_found;

                    cursor = (cursor + i) - delta1[buf[cursor + i]];
                    ok = false;
                    break;
                }
                ok = true;
            }
        }
    }

    if (ok)
        return cursor;

    not_found:
    return -1;
}