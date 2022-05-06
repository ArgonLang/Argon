// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_SUPPORT_FORMATTER_H_
#define ARGON_OBJECT_SUPPORT_FORMATTER_H_

#include <utils/enum_bitmask.h>

#include <object/arobject.h>

namespace argon::object::support {
    enum class FormatFlags {
        NONE = 0x00,
        LJUST = 0x01,
        SIGN = 0x02,
        BLANK = 0x04,
        ALT = 0x08,
        ZERO = 0x10
    };

    class Formatter {
        struct {
            const unsigned char *buf = nullptr;
            ArSize len = 0;
            ArSize idx = 0;

            ArObject *args = nullptr;
            ArSize args_idx = 0;
            ArSize args_len = 0;
            int nspec = 0;

            // Current output information
            FormatFlags flags = FormatFlags::NONE;
            int prec = 0;
            int width = 0;
        } fmt;

        unsigned char *str_ = nullptr;
        ArSize cap_ = 0;
        ArSize idx_ = 0;

        ArObject *NextArg();

        bool BufferResize(ArSize sz);

        bool DoFormat();

        bool ParseStarOption(bool prec);

        bool WriteRepeat(unsigned char chr, int times);

        int FormatBytesString();

        int FormatChar();

        int FormatDecimal(unsigned char specifier);

        int FormatInteger(int base, bool upper);

        static int
        FormatNumber(unsigned char *buf, int index, int base, int width, bool upper, bool neg, FormatFlags flags);

        int FormatString();

        int ParseNextSpecifier();

        int Write(const unsigned char *buf, int sz, int overalloc);

        int Write(const unsigned char *buf, int sz) {
            return this->Write(buf, sz, 0);
        }

        int WriteNumber(unsigned char *buf, long num, int base, int prec, int width, bool upper, FormatFlags flags);

        void ParseOption();

    public:
        bool string_as_bytes = false;

        Formatter(const char *fmt, ArObject *args);

        Formatter(const char *fmt, ArSize len, ArObject *args);

        ~Formatter();

        ArSize GetCapacity();

        unsigned char *format(ArSize *out_len);

        void ReleaseBufferOwnership();
    };

} // namespace argon::object::support

ENUMBITMASK_ENABLE(argon::object::support::FormatFlags);

#endif // !ARGON_OBJECT_SUPPORT_FORMATTER_H_
