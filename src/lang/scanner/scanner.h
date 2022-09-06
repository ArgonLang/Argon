// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SCANNER_SCANNER_H_
#define ARGON_LANG_SCANNER_SCANNER_H_

#include <cstdio>
#include <cstring>

#include "ibuffer.h"
#include "token.h"
#include "sbuffer.h"

namespace argon::lang::scanner {
    constexpr auto kScannerFileBuffer = 2048;
    constexpr auto kScannerPromptBuffer = 1024;

    enum class ScannerStatus {
        EMPTY_SQUOTE,
        END_OF_FILE,
        INVALID_BINARY_LITERAL,
        INVALID_BSTR,
        INVALID_BYTE_ULONG,
        INVALID_BYTE_USHORT,
        INVALID_HEX_BYTE,
        INVALID_HEX_LITERAL,
        INVALID_LC,
        INVALID_OCTAL_LITERAL,
        INVALID_RSTR,
        INVALID_RS_PROLOGUE,
        INVALID_SQUOTE,
        INVALID_STR,
        INVALID_TK,
        INVALID_UCHR,
        INVALID_U_NUM,
        NOMEM,
        GOOD
    };

    using InteractiveFn = int (*)(const char *prompt, FILE *fd, InputBuffer *ibuf);

    class Scanner {
        const char *prompt_ = nullptr;
        const char *next_prompt_ = nullptr;

        FILE *fd_ = nullptr;

        InteractiveFn promptfn_ = nullptr;

        StoreBuffer sbuf_;

        InputBuffer ibuf_;

        ScannerStatus status_ = ScannerStatus::GOOD;

        Position loc{1, 1, 0};

        int HexToByte();

        bool ParseEscape(int stop, bool ignore_unicode);

        bool ParseHexEscape();

        bool ParseOctEscape(int value);

        bool ParseUnicode(bool extended);

        bool TokenizeAtom(Token *out_token);

        bool TokenizeBinary(Token *out_token);

        bool TokenizeChar(Token *out_token);

        bool TokenizeComment(Token *out_token, bool inline_comment);

        bool TokenizeDecimal(Token *out_token, TokenType type, bool begin_zero);

        bool TokenizeHex(Token *out_token);

        bool TokenizeNumber(Token *out_token);

        bool TokenizeOctal(Token *out_token);

        bool TokenizeRawString(Token *out_token);

        bool TokenizeString(Token *out_token, bool byte_string);

        bool TokenizeWord(Token *out_token);

        int Next() { return this->Peek(true); }

        int Peek(bool advance);

        int Peek() { return this->Peek(false); }

        int UnderflowInteractive();

    public:
        /**
         * @brief Initialize the scanner using a string and length.
         *
         * @param str Pointer to the string that contains the source code.
         * @param length Length of the string that contains the source code.
         */
        Scanner(const char *str, unsigned long length) noexcept: ibuf_((unsigned char *) str, length) {}

        /**
         * @brief Initialize the scanner using a string.
         *
         * @param str Pointer to the string that contains the source code.
         */
        explicit Scanner(const char *str) noexcept: Scanner(str, strlen(str)) {};

        /**
         * @brief Initialize the scanner using a file to read from and prompts to show (interactive mode).
         *
         * @param ps1 Pointer to Prompt 1.
         * @param ps2 Pointer to Prompt 2.
         * @param fd Pointer to FILE.
         */
        Scanner(const char *ps1, const char *ps2, FILE *fd) noexcept;

        /**
         * @brief Reads the next token from the stream and returns it.
         *
         * @param out_token Pointer to the token to fill.
         * @return True in case of success, false otherwise (you can use GetStatusMessage to know the error).
         */
        bool NextToken(Token *out_token) noexcept;

        /**
         * @brief Get Scanner status message.
         *
         * @return Scanner status message.
         */
        [[nodiscard]] const char *GetStatusMessage() const;
    };
} // namespace argon::lang::scanner

#endif // !ARGON_LANG_SCANNER_SCANNER_H_
