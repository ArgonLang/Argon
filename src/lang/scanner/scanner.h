// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SCANNER_SCANNER2_H
#define ARGON_LANG_SCANNER_SCANNER2_H

#include <cstdio>
#include <cstring>

#include "token.h"

#define ARGON_LANG_SCANNER_FILE_BUFSIZ      1024
#define ARGON_LANG_SCANNER_PROMPT_BUFSIZ    256

namespace argon::lang::scanner {

    enum class ScannerStatus {
        EMPTY_SQUOTE,
        INVALID_BSTR,
        INVALID_BYTE_ULONG,
        INVALID_BYTE_USHORT,
        INVALID_HEX_BYTE,
        INVALID_LC,
        INVALID_RSTR,
        INVALID_RS_PROLOGUE,
        INVALID_SQUOTE,
        INVALID_STR,
        INVALID_TK,
        INVALID_UCHR,
        NOMEM,
        GOOD
    };

    using InteractiveFn = int (*)(FILE *fd, const char *prompt, void *buf, int len);

    class Scanner {
        const char *prompt_ = nullptr;
        const char *next_prompt_ = nullptr;

        struct {
            unsigned char *start_ = nullptr;
            unsigned char *cur_ = nullptr;
            unsigned char *inp_ = nullptr;
            unsigned char *end_ = nullptr;
        } buffers;

        struct {
            unsigned char *start_ = nullptr;
            unsigned char *cur_ = nullptr;
            unsigned char *end_ = nullptr;
        } tkval;

        FILE *fd_ = nullptr;

        InteractiveFn promptfn_ = nullptr;

        Pos pos_ = 1;

        bool TkEnlarge(int len);

        bool TkInitBuf();

        bool TkPutChar(int value);

        bool TkPutChar();

        bool TkPutStr(const unsigned char *str, int len);

        bool ProcessEscape(int stop, bool ignore_unicode);

        bool ProcessEscapeHex();

        bool ProcessEscapeOct(int value);

        bool ProcessUnicode(bool extended);

        bool UnderflowFile();

        bool UnderflowInteractive();

        int ExpandBuffer(int newsize);

        int HexToByte();

        int Peek(bool advance);

        int PeekChar() noexcept;

        int NextChar() noexcept;

        Token MakeTkWithValue(Pos start, TokenType type);

        Token TokenizeBinary(Pos start);

        Token TokenizeChar(Pos start);

        Token TokenizeComment(Pos start, bool inline_comment);

        Token TokenizeDecimal(Pos start, bool begin_zero);

        Token TokenizeHex(Pos start);

        Token TokenizeNumber();

        Token TokenizeOctal(Pos start);

        Token TokenizeRawString(Pos start);

        Token TokenizeString(Pos start, bool byte_string);

        Token TokenizeWord();

        [[nodiscard]] unsigned char *TkGetValue();

    public:
        ScannerStatus status = ScannerStatus::GOOD;

        Scanner(FILE *fd, const char *ps1, const char *ps2) noexcept;

        Scanner(const char *str, unsigned long len) noexcept;

        explicit Scanner(const char *str) noexcept: Scanner(str, strlen(str)) {};

        ~Scanner() noexcept;

        bool Reset();

        const char *GetStatusMessage();

        Token NextToken() noexcept;

        void SetPromptFn(InteractiveFn fn);
    };
}

#endif // !ARGON_LANG_SCANNER_SCANNER2_H
