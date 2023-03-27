// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_STRINGFORMATTER_H_
#define ARGON_VM_DATATYPE_STRINGFORMATTER_H_

#include <argon/util/enum_bitmask.h>

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/error.h>

namespace argon::vm::datatype {
    enum class FormatFlags {
        NONE = 0x00,
        LJUST = 0x01,
        SIGN = 0x02,
        BLANK = 0x04,
        ALT = 0x08,
        ZERO = 0x10
    };

    class StringFormatter {
        ArObject *error_ = nullptr;

        struct {
            const unsigned char *cursor = nullptr;
            const unsigned char *end = nullptr;

            ArObject *args = nullptr;
            ArSize args_index = 0;
            ArSize args_length = 0;
            int nspec = 0;

            FormatFlags flags = FormatFlags::NONE;
            int width = 0;
            int prec = 0;
        } fmt_;

        struct {
            unsigned char *buffer = nullptr;
            unsigned char *cursor = nullptr;
            unsigned char *end = nullptr;
        } output_;

        bool string_as_bytes_ = false;

        ArObject *NextArg();

        ArSSize FormatBytes();

        ArSSize FormatString();

        ArSSize Write(const unsigned char *buffer, ArSize length, int overalloc);

        ArSSize WriteRepeat(char ch, int times);

        bool BufferResize(ArSize length);

        bool Format();

        int FormatChar();

        int FormatDecimal(unsigned char op);

        int FormatInteger(int base, bool unsign, bool upper);

        bool NextSpecifier();

        bool ParseOption();

        bool ParseStarOption(bool prec);

        static int FormatNumber(unsigned char *buf, int index, int base, int width, bool upper,
                                bool neg, FormatFlags flags);

        template<typename T>
        static int WriteNumber(unsigned char *buf, T num, int base, int prec, int width,
                               bool upper, FormatFlags flags) {
            static unsigned char l_case[] = "0123456789abcdef";
            static unsigned char u_case[] = "0123456789ABCDEF";
            const unsigned char *p_case = upper ? u_case : l_case;

            int index = 0;
            bool neg = false;

            if (num < 0) {
                num = -num;
                neg = true;
            } else if (num == 0)
                buf[index++] = '0';

            while (num) {
                buf[index++] = p_case[num % base];
                num /= base;
            }

            if (prec > index) {
                prec -= index;
                while (prec--)
                    buf[index++] = '0';
            }

            return StringFormatter::FormatNumber(buf, index, base, width, upper, neg, flags);
        }

    public:
        StringFormatter(const char *fmt, ArObject *args, bool string_as_bytes) : string_as_bytes_(string_as_bytes) {
            this->fmt_.cursor = (const unsigned char *) fmt;
            this->fmt_.end = this->fmt_.cursor + strlen(fmt);
            this->fmt_.args = args;
        }

        StringFormatter(const char *fmt, ArSize length, ArObject *args, bool string_as_bytes) :
                string_as_bytes_(string_as_bytes) {
            this->fmt_.cursor = (const unsigned char *) fmt;
            this->fmt_.end = this->fmt_.cursor + length;
            this->fmt_.args = args;
        }

        ~StringFormatter();

        ArObject *GetError();

        unsigned char *Format(ArSize *out_len, ArSize *out_cap);

        void ReleaseOwnership();
    };
}

ENUMBITMASK_ENABLE(argon::vm::datatype::FormatFlags);

#endif // !ARGON_VM_DATATYPE_STRINGFORMATTER_H_
