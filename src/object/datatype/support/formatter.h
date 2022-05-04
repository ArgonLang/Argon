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

        unsigned char *str = nullptr;
        ArSize len =0;
        ArSize idx = 0;

        ArObject *NextArg();

        int ParseNextSpecifier();

        void ParseOption();

        bool ParseStarOption(bool prec);

        bool DoFormat();

        int Write(const unsigned char *buf, int sz, int overalloc);

        int Write(const unsigned char *buf, int sz) {
            return this->Write(buf, sz, 0);
        }

        int WriteNumber(unsigned char *buf, long num, int base, int prec, int width, bool upper,FormatFlags flags);

        bool WriteRepeat(unsigned char chr, int times);

        bool BufferResize(ArSize sz);

        int FormatString();

        int FormatInteger(int base, bool upper);

        int FormatDecimal(unsigned char specifier);

        int FormatNumber(unsigned char *buf, int index, int base, int width, bool upper, bool neg, FormatFlags flags);

        int FormatChar();

    public:
        Formatter(const char *fmt, ArObject *args);

        [[nodiscard]] unsigned char *format(ArSize *out_len);
    };

} // namespace argon::object::support

ENUMBITMASK_ENABLE(argon::object::support::FormatFlags);

#endif // !ARGON_OBJECT_SUPPORT_FORMATTER_H_
