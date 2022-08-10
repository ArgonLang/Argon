// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/datatype/stringbuilder.h>
#include <vm/memory/memory.h>

#include "scanner.h"

using namespace argon::lang::scanner;

struct KwToken {
    const char *keyword;
    TokenType type;
};

constexpr KwToken kw2tktype[] = {
        {"as", TokenType::KW_AS},
        {"assert", TokenType::KW_ASSERT}
};

int DefaultPrompt(const char *prompt, FILE *fd, InputBuffer *ibuf) {
    int length = kScannerPromptBuffer;
    int cur = 0;

    unsigned char *buf = nullptr;
    unsigned char *tmp;

    printf("%s", prompt);

    do {
        length += cur >> 1;

        if ((tmp = (unsigned char *) argon::vm::memory::Realloc(buf, length)) == nullptr) {
            cur = -1;
            argon::vm::memory::Free(buf);
            return cur;
        }

        buf = tmp;

        if (std::fgets((char *) buf + cur, length - cur, fd) == nullptr) {
            cur = -1;

            if (feof(fd) != 0)
                cur = 0;

            argon::vm::memory::Free(buf);
            return cur;
        }

        cur += (int) strlen((char *) buf + cur);
    } while (*((buf + cur) - 1) != '\n');

    if (!ibuf->AppendInput(buf, cur))
        cur = -1;

    argon::vm::memory::Free(buf);
    return cur;
}

inline unsigned char HexDigitToNumber(int chr) {
    return (isdigit(chr)) ? ((char) chr) - '0' : (unsigned char) (10 + (tolower(chr) - 'a'));
}

inline bool IsOctDigit(int chr) { return chr >= '0' && chr <= '7'; }

int Scanner::HexToByte() {
    int byte = 0;
    int curr;

    for (int i = 1; i >= 0; i--) {
        curr = this->Next();

        if (!ishexnumber(curr))
            return -1;

        byte |= HexDigitToNumber(curr) << (unsigned char) (i * 4);
    }

    return byte;
}

bool Scanner::ParseEscape(int stop, bool ignore_unicode) {
    int value = this->Next();
    bool ok = true;

    if (value == stop) {
        if (!this->sbuf_.PutChar((unsigned char) value)) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }

        return true;
    }

    if (!ignore_unicode) {
        if (value == 'u')
            return this->ParseUnicode(false);

        if (value == 'U')
            return this->ParseUnicode(true);
    }

    switch (value) {
        case 'a':
            ok = this->sbuf_.PutChar(0x07);
            break;
        case 'b':
            ok = this->sbuf_.PutChar(0x08);
            break;
        case 'f':
            ok = this->sbuf_.PutChar(0x0C);
            break;
        case 'n':
            ok = this->sbuf_.PutChar(0x0A);
            break;
        case 'r':
            ok = this->sbuf_.PutChar(0x0D);
            break;
        case 't':
            ok = this->sbuf_.PutChar(0x09);
            break;
        case 'v':
            ok = this->sbuf_.PutChar(0x0B);
            break;
        case 'x':
            return this->ParseHexEscape();
        default:
            if (IsOctDigit(value))
                return this->ParseOctEscape(value);

            ok = this->sbuf_.PutChar('\\');
            if (ok)
                this->sbuf_.PutChar((unsigned char) value);
    }

    if (!ok) {
        this->status_ = ScannerStatus::NOMEM;
        return false;
    }

    return true;
}

bool Scanner::ParseHexEscape() {
    int byte;

    if ((byte = this->HexToByte()) < 0) {
        this->status_ = ScannerStatus::INVALID_HEX_BYTE;
        return false;
    }

    if (!this->sbuf_.PutChar((unsigned char) byte)) {
        this->status_ = ScannerStatus::NOMEM;
        return false;
    }

    return true;
}

bool Scanner::ParseOctEscape(int value) {
    unsigned char sequence[] = {0, 0, 0};
    unsigned char byte = 0;

    if (!IsOctDigit(value))
        return false;

    sequence[2] = HexDigitToNumber(value);

    for (int i = 1; i >= 0 && IsOctDigit(this->Peek()); i--)
        sequence[i] = HexDigitToNumber(this->Next());

    for (int i = 0, mul = 0; i < 3; i++) {
        byte |= sequence[i] << (unsigned char) (mul * 3);
        if (sequence[i] != 0)
            mul++;
    }

    if (!this->sbuf_.PutChar(byte)) {
        this->status_ = ScannerStatus::NOMEM;
        return false;
    }

    return true;
}

bool Scanner::ParseUnicode(bool extended) {
    unsigned char sequence[] = {0, 0, 0, 0};
    unsigned char buf[4] = {};
    int width = 2;
    int byte;
    int len;

    if (extended)
        width = 4;

    for (int i = 0; i < width; i++) {
        if ((byte = this->HexToByte()) < 0) {
            this->status_ = ScannerStatus::INVALID_BYTE_USHORT;
            if (extended)
                this->status_ = ScannerStatus::INVALID_BYTE_ULONG;
            return false;
        }
        sequence[(width - 1) - i] = (unsigned char) byte;
    }

    len = argon::vm::datatype::StringIntToUTF8(*((unsigned int *) sequence), buf);
    if (len == 0) {
        this->status_ = ScannerStatus::INVALID_UCHR;
        return false;
    }

    if (!this->sbuf_.PutString(buf, len)) {
        this->status_ = ScannerStatus::NOMEM;
        return false;
    }

    return true;
}

bool Scanner::TokenizeBinary(Token *out_token) {
    int value = this->Peek();

    while (value >= '0' && value <= '1') {
        if (!this->sbuf_.PutChar((unsigned char) this->Next())) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }

        value = this->Peek();
    }

    if (isdigit(value)) {
        this->status_ = ScannerStatus::INVALID_BINARY_LITERAL;
        return false;
    }

    out_token->type = TokenType::NUMBER_BIN;
    out_token->loc.end = this->loc;
    out_token->length = this->sbuf_.GetBuffer(&out_token->buffer);
    return true;
}

bool Scanner::TokenizeDecimal(Token *out_token, TokenType type, bool begin_zero) {
    if (begin_zero && !this->sbuf_.PutChar('0')) {
        this->status_ = ScannerStatus::NOMEM;
        return false;
    }

    if (type == TokenType::DECIMAL && !this->sbuf_.PutChar('.')) {
        this->status_ = ScannerStatus::NOMEM;
        return false;
    }

    while (isdigit(this->Peek())) {
        if (!this->sbuf_.PutChar((unsigned char) this->Next())) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }
    }

    // Look for a fractional part.
    if (this->Peek() == '.' && type != TokenType::DECIMAL) {
        if (!this->sbuf_.PutChar((unsigned char) this->Next())) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }

        while (isdigit(this->Peek())) {
            if (!this->sbuf_.PutChar((unsigned char) this->Next())) {
                this->status_ = ScannerStatus::NOMEM;
                return false;
            }
        }

        type = TokenType::DECIMAL;
    }

    out_token->type = type;
    out_token->loc.end = this->loc;
    out_token->length = this->sbuf_.GetBuffer(&out_token->buffer);
    return true;
}

bool Scanner::TokenizeHex(Token *out_token) {
    int value = this->Peek();

    while (ishexnumber(value)) {
        if (!this->sbuf_.PutChar((unsigned char) this->Next())) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }

        value = this->Peek();
    }

    if (isalpha(value)) {
        this->status_ = ScannerStatus::INVALID_HEX_LITERAL;
        return false;
    }

    out_token->type = TokenType::NUMBER_HEX;
    out_token->loc.end = this->loc;
    out_token->length = this->sbuf_.GetBuffer(&out_token->buffer);
    return true;
}

bool Scanner::TokenizeNumber(Token *out_token) {
    bool begin_zero = false;

    if (this->Peek() == '0') {
        this->Next();

        switch (tolower(this->Peek())) {
            case 'b':
                this->Next();
                return this->TokenizeBinary(out_token);
            case 'o':
                this->Next();
                return this->TokenizeOctal(out_token);
            case 'x':
                this->Next();
                return this->TokenizeHex(out_token);
            default:
                begin_zero = true;
                break;
        }
    }

    return this->TokenizeDecimal(out_token, TokenType::NUMBER, begin_zero);
}

bool Scanner::TokenizeOctal(Token *out_token) {
    int value = this->Peek();

    while (IsOctDigit(value)) {
        if (!this->sbuf_.PutChar((unsigned char) this->Next())) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }

        value = this->Peek();
    }

    if (isdigit(value)) {
        this->status_ = ScannerStatus::INVALID_OCTAL_LITERAL;
        return false;
    }

    out_token->type = TokenType::NUMBER_OCT;
    out_token->loc.end = this->loc;
    out_token->length = this->sbuf_.GetBuffer(&out_token->buffer);
    return true;
}

bool Scanner::TokenizeRawString(Token *out_token) {
    int hashes = 0;
    int value;

    // Count beginning hashes
    for (; this->Peek() == '#'; this->Next(), hashes++);

    if (this->Next() != '"') {
        this->status_ = ScannerStatus::INVALID_RS_PROLOGUE;
        return false;
    }

    while ((value = this->Peek()) > 0) {
        int count = 0;

        if (value == '"') {
            this->Next();

            for (; this->Peek() == '#'; this->Next(), count++);

            if (count == hashes) {
                out_token->type = TokenType::RAW_STRING;
                out_token->loc.end = this->loc;
                out_token->length = this->sbuf_.GetBuffer(&out_token->buffer);
                return true;
            }

            if (!this->sbuf_.PutChar('"')) {
                this->status_ = ScannerStatus::NOMEM;
                return false;
            }

            if (!this->sbuf_.PutCharRepeat('#', count)) {
                this->status_ = ScannerStatus::NOMEM;
                return false;
            }
        }

        if (!this->sbuf_.PutChar((unsigned char) this->Next())) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }
    }

    this->status_ = ScannerStatus::INVALID_RSTR;
    return false;
}

bool Scanner::TokenizeString(Token *out_token, bool byte_string) {
    TokenType type = TokenType::STRING;
    int value = this->Next();

    if (byte_string)
        type = TokenType::BYTE_STRING;

    while (value != '"') {
        if (value < 0 || value == '\n') {
            this->status_ = ScannerStatus::INVALID_STR;
            return false;
        }

        // Byte string accept byte in range (0x00 - 0x7F)
        if (byte_string && value > 0x7F) {
            this->status_ = ScannerStatus::INVALID_BSTR;
            return false;
        }

        if (value == '\\') {
            if (this->Peek() != '\\') {
                if (!this->ParseEscape('"', byte_string))
                    return false;

                value = this->Next();
                continue;
            }

            value = this->Next();
        }

        if (!this->sbuf_.PutChar((unsigned char) value)) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }

        value = this->Next();
    }

    out_token->type = type;
    out_token->loc.end = this->loc;
    out_token->length = this->sbuf_.GetBuffer(&out_token->buffer);
    return true;
}

bool Scanner::TokenizeWord(Token *out_token) {
    int value = this->Next();

    if (value == 'b' && this->Peek() == '"') {
        this->Next(); // Discard "
        return this->TokenizeString(out_token, true);
    }

    if (value == 'r' && (this->Peek() == '#' || this->Peek() == '"'))
        return this->TokenizeRawString(out_token);

    while (isalnum(value) || value == '_') {
        if (!this->sbuf_.PutChar((unsigned char) value)) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }
        value = this->Next();
    }

    out_token->type = TokenType::IDENTIFIER;
    out_token->loc.end = this->loc;
    out_token->length = this->sbuf_.GetBuffer(&out_token->buffer);

    if (out_token->length == 1 && *out_token->buffer == '_')
        out_token->type = TokenType::BLANK;

    // keywords are longer than one letter
    if (out_token->length > 1) {
        for (KwToken kt: kw2tktype) {
            auto delta = out_token->length;
            if (argon::vm::memory::MemoryCompare(kt.keyword, out_token->buffer, delta) == 0) {
                out_token->type = kt.type;
                break;
            }
        }
    }

    return true;
}

int Scanner::UnderflowInteractive() {
    int err = this->promptfn_(this->prompt_, this->fd_, &this->ibuf_);
    if (err < 0)
        return err;

    if (this->next_prompt_ != nullptr)
        this->prompt_ = this->next_prompt_;

    return err;
}

int Scanner::Peek(bool advance) {
    int chr;
    int err;

    do {
        if ((chr = this->ibuf_.Peek(advance)) > 0)
            break;

        if (this->fd_ == nullptr)
            return 0;

        if (this->prompt_ != nullptr)
            err = this->UnderflowInteractive();
        else
            err = this->ibuf_.ReadFile(this->fd_, kScannerFileBuffer);

        if (err == 0) {
            this->status_ = ScannerStatus::END_OF_FILE;
            return 0;
        }

        if (err < 0)
            this->status_ = ScannerStatus::NOMEM;

        chr = err;
    } while (chr > 0);

    if (advance) {
        this->loc.offset++;
        this->loc.column++;

        if (chr == '\n') {
            this->loc.line++;
            this->loc.column = 1;
        }
    }

    return chr;
}

Scanner::Scanner(const char *ps1, const char *ps2, FILE *fd) noexcept: prompt_(ps1), next_prompt_(ps2), fd_(fd) {
    this->promptfn_ = DefaultPrompt;
}

bool Scanner::NextToken(Token *out_token) noexcept {
    int value;

    // Reset error status
    this->status_ = ScannerStatus::GOOD;

    if (out_token->buffer != nullptr) {
        argon::vm::memory::Free(out_token->buffer);

        out_token->buffer = nullptr;
        out_token->length = 0;
    }

    while ((value = this->Peek()) > 0) {
        out_token->loc.start = this->loc;

        // Skip spaces
        if (isblank(value)) {
            while (isblank(this->Peek()))
                this->Next();

            continue;
        }

        if (isalpha(value) || value == '_')
            return this->TokenizeWord(out_token);

        if (isdigit(value))
            return this->TokenizeNumber(out_token);

        this->Next();

        switch (value) {
            case '\n':
                while (this->Peek() == '\n')
                    this->Next();
                out_token->loc.end = this->loc;
                out_token->type = TokenType::END_OF_LINE;
                return true;
            case '.':
                if (isdigit(this->Peek()))
                    return this->TokenizeDecimal(out_token, TokenType::DECIMAL, false);
                assert(false);
        }
    }

    out_token->loc.start = this->loc;
    out_token->loc.end = this->loc;
    out_token->type = TokenType::END_OF_FILE;
    return true;
}