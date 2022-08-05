// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_STRINGBUILDER_H_
#define ARGON_VM_DATATYPE_STRINGBUILDER_H_

#include "error.h"
#include "arstring.h"

namespace argon::vm::datatype {
    class StringBuilder {
        Error *error_ = nullptr;
        unsigned char *buffer_ = nullptr;

        ArSize cap_ = 0;
        ArSize len_ = 0;
        ArSize cp_len_ = 0;

        StringKind kind_ = StringKind::ASCII;

        static ArSize GetEscapedLength(const unsigned char *buffer, ArSize length, bool unicode);

        static ArSize GetUnescapedLength(const unsigned char *buffer, ArSize length);

        int HexToByte(const unsigned char *buffer, ArSize length);

        int ProcessUnicodeEscape(unsigned char *wb, const unsigned char *buffer, ArSize length, bool extended);

    public:
        ~StringBuilder();

        bool BufferResize(ArSize sz);

        bool ParseEscaped(const unsigned char *buffer, ArSize length);

        bool Write(const unsigned char *buffer, ArSize length, ArSize overalloc);

        bool Write(const String *string, ArSize overalloc) {
            return this->Write(string->buffer, string->length, overalloc);
        }

        bool WriteEscaped(const unsigned char *buffer, ArSize length, ArSize overalloc, bool unicode);

        bool WriteEscaped(const unsigned char *buffer, ArSize length, ArSize overalloc) {
            return this->WriteEscaped(buffer, length, overalloc, false);
        }

        bool WriteHex(const unsigned char *buffer, ArSize length);

        bool WriteRepeat(char ch, int times);

        Error *GetError();

        String *BuildString();
    };

    bool CheckUnicodeCharSequence(StringKind *out_kind, ArSize *out_uidx, Error **out_error,
                                  unsigned char chr, ArSize index);

    int StringIntToUTF8(unsigned int glyph, unsigned char *buf);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_STRINGBUILDER_H_
