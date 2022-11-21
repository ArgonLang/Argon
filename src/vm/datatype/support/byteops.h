// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_SUPPORT_BYTEOPS_H_
#define ARGON_VM_DATATYPE_SUPPORT_BYTEOPS_H_

#include <vm/datatype/objectdef.h>

namespace argon::vm::datatype::support {
    ArSSize Count(const unsigned char *buf, ArSize blen, const unsigned char *pattern, ArSize plen, long n);

    [[maybe_unused]]
    inline ArSSize Count(const unsigned char *buf, ArSize blen, const unsigned char *pattern, ArSize plen) {
        return Count(buf, blen, pattern, plen, -1);
    }

    ArSSize FindNewLine(const unsigned char *buf, ArSize *inout_len, bool universal);

    [[maybe_unused]]
    inline ArSSize FindNewLine(const unsigned char *buf, ArSize *inout_len) {
        return FindNewLine(buf,
                           inout_len,
#ifdef ARGON_FF_UNIVERSAL_NEWLINE
                true
#else
        false
#endif
        );
    }

    long Find(const unsigned char *buf, ArSize blen, const unsigned char *pattern, ArSize plen, bool reverse);

    [[maybe_unused]]
    inline long Find(const unsigned char *buf, ArSize blen, const unsigned char *pattern, ArSize plen) {
        return Find(buf, blen, pattern, plen, false);
    }

    long FindWhitespace(const unsigned char *buf, ArSize *inout_len, bool reverse);

    [[maybe_unused]]
    inline long FindWhitespace(const unsigned char *buf, ArSize *inout_len) {
        return FindWhitespace(buf, inout_len, false);
    }

} // namespace argon::vm::datatype::support

#endif // !ARGON_VM_DATATYPE_SUPPORT_BYTEOPS_H_
