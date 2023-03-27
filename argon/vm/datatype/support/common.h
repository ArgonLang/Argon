// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_SUPPORT_COMMON_H_
#define ARGON_VM_DATATYPE_SUPPORT_COMMON_H_

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/list.h>

#include <argon/vm/datatype/support/byteops.h>

namespace argon::vm::datatype::support {
    template<typename T>
    using SplitChunkNewFn = T *(*)(const unsigned char *, ArSize length);

    template<typename T>
    ArObject *Split(const unsigned char *buffer, const unsigned char *pattern, SplitChunkNewFn<T> tp_new,
                    ArSize blen, ArSize plen, ArSSize maxsplit) {
        T *tmp;
        List *ret;
        ArSize cursor;
        ArSSize occurrence;
        ArSSize start;

        bool whitespace = false;

        if (pattern == nullptr || plen == 0) {
            occurrence = CountWhitespace(buffer, blen);
            whitespace = true;
        } else
            occurrence = support::Count(buffer, blen, pattern, plen);

        if ((ret = ListNew(occurrence + 1)) == nullptr)
            return nullptr;

        cursor = 0;

        if (!whitespace)
            start = support::Find(buffer, blen, pattern, plen);
        else {
            plen = blen;
            start = FindWhitespace(buffer, &plen);
        }

        while (start > -1 && maxsplit != 0) {
            tmp = tp_new(buffer + cursor, start);
            cursor += start + plen;

            if (tmp == nullptr) {
                Release(ret);
                return nullptr;
            }

            ListAppend(ret, (ArObject *) tmp);

            Release(tmp);

            if (!whitespace)
                start = support::Find(buffer + cursor, blen-cursor, pattern, plen);
            else {
                plen = blen - cursor;
                start = FindWhitespace(buffer+cursor, &plen);
            }

            if (maxsplit > 0)
                maxsplit--;
        }

        if (blen - cursor > 0) {
            tmp = tp_new(buffer + cursor, blen - cursor);
            if (tmp == nullptr) {
                Release(ret);
                return nullptr;
            }

            ListAppend(ret, (ArObject *) tmp);

            Release(tmp);
        }

        return (ArObject *) ret;
    }
}

#endif // !ARGON_VM_DATATYPE_SUPPORT_COMMON_H_
