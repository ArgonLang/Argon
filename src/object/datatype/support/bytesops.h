// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_SUPPORT_BYTESOPS_H_
#define ARGON_OBJECT_SUPPORT_BYTESOPS_H_

#include <cstddef>

#include <object/arobject.h>

namespace argon::object::support {

    long Count(const unsigned char *buf, ArSSize blen, const unsigned char *pattern, ArSSize plen, long n);

    inline long Count(const unsigned char *buf, ArSSize blen, const unsigned char *pattern, ArSSize plen) {
        return Count(buf, blen, pattern, plen, 0);
    }

    long Find(const unsigned char *buf, ArSSize blen, const unsigned char *pattern, ArSSize plen, bool reverse);

    inline long Find(const unsigned char *buf, ArSSize blen, const unsigned char *pattern, ArSSize plen) {
        return Find(buf, blen, pattern, plen, false);
    }

} // namespace argon::object::support

#endif // !ARGON_OBJECT_SUPPORT_BYTESOPS_H_
