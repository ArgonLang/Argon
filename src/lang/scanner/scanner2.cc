// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <iostream>

#include <memory/memory.h>

#include "scanner2.h"

using namespace argon::lang::scanner2;

struct KwToken {
    const char *keyword;
    TokenType type;
};

KwToken kw2tktype[] = {
        {"as",          TokenType::AS},
        {"break",       TokenType::BREAK},
        {"case",        TokenType::CASE},
        {"continue",    TokenType::CONTINUE},
        {"default",     TokenType::DEFAULT},
        {"defer",       TokenType::DEFER},
        {"elif",        TokenType::ELIF},
        {"else",        TokenType::ELSE},
        {"fallthrough", TokenType::FALLTHROUGH},
        {"false",       TokenType::FALSE},
        {"for",         TokenType::FOR},
        {"from",        TokenType::FROM},
        {"func",        TokenType::FUNC},
        //{"goto",        TokenType::GOTO},
        {"if",          TokenType::IF},
        {"in",          TokenType::IN},
        {"impl",        TokenType::IMPL},
        {"import",      TokenType::IMPORT},
        {"let",         TokenType::LET},
        {"loop",        TokenType::LOOP},
        {"nil",         TokenType::NIL},
        {"pub",         TokenType::PUB},
        {"return",      TokenType::RETURN},
        {"self",        TokenType::SELF},
        {"spawn",       TokenType::SPAWN},
        {"struct",      TokenType::STRUCT},
        {"switch",      TokenType::SWITCH},
        {"trait",       TokenType::TRAIT},
        {"true",        TokenType::TRUE},
        {"var",         TokenType::VAR},
        {"weak",        TokenType::WEAK}
};

// UTILITIES

inline bool IsAlpha(int chr) { return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || chr == '_'; }

inline bool IsDigit(int chr) { return chr >= '0' && chr <= '9'; }

inline bool IsHexDigit(int chr) { return (chr >= '0' && chr <= '9') || (tolower(chr) >= 'a' && tolower(chr) <= 'f'); }

inline bool IsOctDigit(int chr) { return chr >= '0' && chr <= '7'; }

inline bool IsSpace(int chr) { return chr == 0x09 || chr == 0x20; }

inline unsigned char HexDigitToNumber(int chr) {
    return (IsDigit(chr)) ? ((char) chr) - '0' : 10 + (tolower(chr) - 'a');
}

int DefaultPrompt(FILE *fd, const char *prompt, void *buf, int blen) {
    printf("%s", prompt);

    if (std::fgets((char *) buf, blen, fd) == nullptr) {
        if (feof(fd) != 0)
            return 0;
        return -1;
    }

    return (int) strlen((char *) buf);
}

// EOL

bool Scanner::TkEnlarge(int len) {
    unsigned char *tmp;
    unsigned long sz;
    unsigned long cur;

    if (this->tkval.end_ - this->tkval.cur_ < len) {
        sz = this->tkval.end_ - this->tkval.start_;
        cur = this->tkval.cur_ - this->tkval.start_;

        tmp = (unsigned char *) argon::memory::Realloc(this->tkval.start_, sz + len);
        if (tmp == nullptr) {
            this->status = ScannerStatus::NOMEM;
            return false;
        }
        this->tkval.start_ = tmp;
        this->tkval.cur_ = this->tkval.start_ + cur;
        this->tkval.end_ = this->tkval.start_ + (sz + len);
    }

    return true;
}

bool Scanner::TkInitBuf() {
    if (this->tkval.start_ == nullptr) {
        this->tkval.start_ = (unsigned char *) argon::memory::Alloc(8);
        if (this->tkval.start_ == nullptr) {
            this->status = ScannerStatus::NOMEM;
            return false;
        }
        this->tkval.cur_ = this->tkval.start_;
        this->tkval.end_ = this->tkval.start_ + 8;
    }

    return true;
}

bool Scanner::TkPutChar(int value) {
    if (!this->TkInitBuf())
        return false;

    do {
        if (this->tkval.cur_ != this->tkval.end_) {
            *this->tkval.cur_ = value;
            this->tkval.cur_++;
            return true;
        }

        if (!this->TkEnlarge(8))
            return false;

    } while (true);
}

bool Scanner::TkPutChar() {
    return this->TkPutChar(this->NextChar());
}

bool Scanner::TkPutStr(const unsigned char *str, int len) {
    if (!this->TkEnlarge(len))
        return false;

    this->tkval.cur_ = (unsigned char *) argon::memory::MemoryCopy(this->tkval.cur_, str, len);

    return true;
}

bool Scanner::ProcessEscape(int stop, bool ignore_unicode) {
    int value = this->NextChar();

    if (value == stop)
        return this->TkPutChar(value);

    if (!ignore_unicode) {
        if (value == 'u')
            return this->ProcessUnicode(false);

        if (value == 'U')
            return this->ProcessUnicode(true);
    }

    switch (value) {
        case 'a':
            return this->TkPutChar(0x07);
        case 'b':
            return this->TkPutChar(0x08);
        case 'f':
            return this->TkPutChar(0x0C);
        case 'n':
            return this->TkPutChar(0x0A);
        case 'r':
            return this->TkPutChar(0x0D);
        case 't':
            return this->TkPutChar(0x09);
        case 'v':
            return this->TkPutChar(0x0B);
        case 'x':
            return this->ProcessEscapeHex();
        default:
            if (!this->ProcessEscapeOct(value)) {
                if (this->TkPutChar('\\'))
                    return this->TkPutChar(value);
                return false;
            }
    }
    return true;
}

bool Scanner::ProcessEscapeHex() {
    int byte;

    if ((byte = this->HexToByte()) < 0) {
        this->status = ScannerStatus::INVALID_HEX_BYTE;
        return false;
    }

    return this->TkPutChar(byte);
}

bool Scanner::ProcessEscapeOct(int value) {
    unsigned char sequence[] = {0, 0, 0};
    unsigned char byte = 0;

    if (!IsOctDigit(value))
        return false;

    sequence[2] = HexDigitToNumber(value);

    for (int i = 1; i >= 0 && IsOctDigit(this->PeekChar()); i--)
        sequence[i] = HexDigitToNumber(this->NextChar());

    for (int i = 0, mul = 0; i < 3; i++) {
        byte |= sequence[i] << (unsigned char) (mul * 3);
        if (sequence[i] != 0)
            mul++;
    }

    return this->TkPutChar(byte);
}

bool Scanner::ProcessUnicode(bool extended) {
    unsigned char sequence[] = {0, 0, 0, 0};
    unsigned char buf[4] = {};

    auto *sq_ptr = (unsigned int *) sequence;
    unsigned char byte;
    int width = 2;
    int len = 1;

    if (extended)
        width = 4;

    for (int i = 0; i < width; i++) {
        if ((byte = this->HexToByte()) < 0) {
            this->status = ScannerStatus::INVALID_BYTE_USHORT;
            if (extended)
                this->status = ScannerStatus::INVALID_BYTE_ULONG;
            return false;
        }
        sequence[(width - 1) - i] = byte;
    }

    if (*sq_ptr < 0x80)
        buf[0] = (*sq_ptr) >> 0 & 0x7F;
    else if (*sq_ptr < 0x0800) {
        buf[0] = (*sq_ptr) >> 6 & 0x1F | 0xC0;
        buf[1] = (*sq_ptr) >> 0 & 0xBF;
        len = 2;
    } else if (*sq_ptr < 0x010000) {
        buf[0] = (*sq_ptr) >> 12 & 0x0F | 0xE0;
        buf[1] = (*sq_ptr) >> 6 & 0x3F | 0x80;
        buf[2] = (*sq_ptr) >> 0 & 0x3F | 0x80;
        len = 3;
    } else if (*sq_ptr < 0x110000) {
        buf[0] = (*sq_ptr) >> 18 & 0x07 | 0xF0;
        buf[1] = (*sq_ptr) >> 12 & 0x3F | 0x80;
        buf[2] = (*sq_ptr) >> 6 & 0x3F | 0x80;
        buf[3] = (*sq_ptr) >> 0 & 0x3F | 0x80;
        len = 4;
    } else {
        this->status = ScannerStatus::INVALID_UCHR;
        return false;
    }

    return this->TkPutStr(buf, len);
}

bool Scanner::UnderflowFile() {
    unsigned long read;
    long rsize;

    // Build circular-buffer
    if (this->buffers.start_ == nullptr) {
        this->buffers.start_ = (unsigned char *) argon::memory::Alloc(ARGON_LANG_SCANNER_FILE_BUFSIZ);

        if (this->buffers.start_ == nullptr)
            return false;

        this->buffers.cur_ = this->buffers.start_;
        this->buffers.inp_ = this->buffers.start_;
        this->buffers.end_ = this->buffers.start_ + ARGON_LANG_SCANNER_FILE_BUFSIZ;
    }

    // Forward reading
    rsize = this->buffers.end_ - this->buffers.inp_;
    if (rsize > 0) {
        read = std::fread(this->buffers.inp_, 1, rsize, this->fd_);

        if (ferror(this->fd_) != 0)
            return false;

        this->buffers.inp_ += read;

        if (feof(this->fd_) != 0)
            return true;
    }

    // Backward reading
    rsize = this->buffers.cur_ - this->buffers.start_;

    if (rsize > 0) {
        read = std::fread(this->buffers.start_, 1, rsize, this->fd_);

        if (ferror(this->fd_) != 0)
            return false;

        this->buffers.inp_ = this->buffers.start_ + read;
    }

    return true;
}

bool Scanner::UnderflowInteractive() {
    int bufsz = this->ExpandBuffer(ARGON_LANG_SCANNER_PROMPT_BUFSIZ);

    bufsz = this->promptfn_(this->fd_, this->prompt_, this->buffers.inp_, bufsz);

    if (bufsz == -1) {
        this->status = ScannerStatus::NOMEM;
        return false;
    }

    this->buffers.inp_ += bufsz;

    if (this->next_prompt_ != nullptr)
        this->prompt_ = this->next_prompt_;

    return true;
}

int Scanner::ExpandBuffer(int newsize) {
    unsigned char *tmp;
    long oldsize;
    long cur;
    long inp;
    long end;

    if (this->buffers.start_ == nullptr) {
        this->buffers.start_ = (unsigned char *) argon::memory::Alloc(newsize);

        if (this->buffers.start_ == nullptr) {
            this->status = ScannerStatus::NOMEM;
            return -1;
        }

        this->buffers.cur_ = this->buffers.start_;
        this->buffers.inp_ = this->buffers.start_;
        this->buffers.end_ = this->buffers.start_ + newsize;
        return newsize;
    }

    oldsize = this->buffers.end_ - this->buffers.inp_;

    if ((newsize >> 1) >= oldsize) {
        cur = this->buffers.cur_ - this->buffers.start_;
        inp = this->buffers.inp_ - this->buffers.start_;
        end = this->buffers.end_ - this->buffers.start_;

        tmp = (unsigned char *) argon::memory::Realloc(this->buffers.start_, end + (newsize - oldsize));
        if (tmp == nullptr) {
            this->status = ScannerStatus::NOMEM;
            return -1;
        }

        this->buffers.start_ = tmp;
        this->buffers.cur_ = tmp + cur;
        this->buffers.inp_ = tmp + inp;
        this->buffers.end_ = tmp + (end + (newsize - oldsize));
    }

    return (int) (newsize - oldsize);
}

int Scanner::HexToByte() {
    int byte = 0;
    int curr;

    for (int i = 1; i >= 0; i--) {
        curr = this->NextChar();

        if (!IsHexDigit(curr))
            return -1;

        byte |= HexDigitToNumber(curr) << (unsigned char) (i * 4);
    }

    return byte;
}

int Scanner::Peek(bool advance) {
    int chr;
    bool ok;

    do {
        if (this->buffers.cur_ != this->buffers.inp_) {
            chr = *this->buffers.cur_;

            if (this->buffers.cur_ >= this->buffers.end_)
                chr = *this->buffers.start_; // Circular buffer

            if (advance) {
                if (this->buffers.cur_ >= this->buffers.end_) {
                    this->buffers.cur_ = this->buffers.start_;
                    return chr;
                }

                this->buffers.cur_++;
                this->pos_++;
            }

            return chr;
        }

        if (this->fd_ == nullptr)
            return -1;

        if (this->prompt_ != nullptr)
            ok = this->UnderflowInteractive();
        else
            ok = this->UnderflowFile();

        if (!ok)
            return -1;

    } while (true);
}

int Scanner::PeekChar() noexcept {
    return this->Peek(false);
}

int Scanner::NextChar() noexcept {
    return this->Peek(true);
}

Token Scanner::MakeTkWithValue(Pos start, TokenType type) {
    unsigned char *tmp = this->TkGetValue();

    if (tmp == nullptr)
        return {TokenType::ERROR, start, this->pos_};

    return {type, start, this->pos_, tmp};
}

Token Scanner::TokenizeBinary(Pos start) {
    int value = this->PeekChar();

    while (value >= '0' && value <= '1') {
        if (!this->TkPutChar())
            return Token(TokenType::ERROR, start, this->pos_);

        value = this->PeekChar();
    }

    return this->MakeTkWithValue(start, TokenType::NUMBER_BIN);
}

Token Scanner::TokenizeChar(Pos start) {
    int value = this->PeekChar();

    if (value == '\'') {
        this->status = ScannerStatus::EMPTY_SQUOTE;
        goto ERROR;
    }

    if (value == '\\') {
        this->NextChar();

        if (this->PeekChar() != '\\') {
            if (!this->ProcessEscape('\'', false))
                goto ERROR;
        } else {
            if (!this->TkPutChar())
                goto ERROR;
        }
    } else {
        if (!this->TkPutChar())
            goto ERROR;
    }

    if (this->NextChar() != '\'') {
        this->status = ScannerStatus::INVALID_SQUOTE;
        goto ERROR;
    }

    return this->MakeTkWithValue(start, TokenType::NUMBER_CHR);

    ERROR:
    return Token(TokenType::ERROR, start, this->pos_);
}

Token Scanner::TokenizeComment(Pos start, bool inline_comment) {
    TokenType type = TokenType::COMMENT;

    if (inline_comment)
        type = TokenType::INLINE_COMMENT;

    // Skip newline/whitespace at comment start
    for (int skip = this->PeekChar();
         IsSpace(skip) || (!inline_comment && skip == '\n');
         this->NextChar(), skip = this->PeekChar());

    while (this->PeekChar() > 0) {
        if (this->PeekChar() == '\n' && inline_comment)
            break;

        if (this->PeekChar() == '*') {
            this->NextChar();

            if (this->PeekChar() == '/')
                break;

            if (!this->TkPutChar('*'))
                goto ERROR;

            continue;
        }

        if (!this->TkPutChar())
            goto ERROR;
    }

    this->NextChar();
    return this->MakeTkWithValue(start, type);

    ERROR:
    return Token(TokenType::ERROR, start, this->pos_);
}

Token Scanner::TokenizeDecimal(Pos start, bool begin_zero) {
    TokenType type = TokenType::NUMBER;

    if (begin_zero) {
        if (!this->TkPutChar('0'))
            return Token(TokenType::ERROR, start, this->pos_);
    }

    while (IsDigit(this->PeekChar())) {
        if (!this->TkPutChar())
            return Token(TokenType::ERROR, start, this->pos_);
    }

    // Look for a fractional part.
    if (this->PeekChar() == '.') {
        if (!this->TkPutChar())
            return Token(TokenType::ERROR, start, this->pos_);

        while (IsDigit(this->PeekChar())) {
            if (!this->TkPutChar())
                return Token(TokenType::ERROR, start, this->pos_);
        }

        type = TokenType::DECIMAL;
    }

    return this->MakeTkWithValue(start, type);
}

Token Scanner::TokenizeHex(Pos start) {
    int value = this->PeekChar();

    while (IsHexDigit(value)) {
        if (!this->TkPutChar())
            return Token(TokenType::ERROR, start, this->pos_);

        value = this->PeekChar();
    }

    return this->MakeTkWithValue(start, TokenType::NUMBER_HEX);
}

Token Scanner::TokenizeOctal(Pos start) {
    int value = this->PeekChar();

    while (value >= '0' && value <= '7') {
        if (!this->TkPutChar())
            return Token(TokenType::ERROR, start, this->pos_);

        value = this->PeekChar();
    }

    return this->MakeTkWithValue(start, TokenType::NUMBER_OCT);
}

Token Scanner::TokenizeNumber() {
    Pos start = this->pos_;
    int peeked;
    bool begin_zero = false;

    if (this->PeekChar() == '0') {
        begin_zero = true;
        this->NextChar();
        switch (tolower(this->PeekChar())) {
            case 'b':
                this->NextChar();
                return this->TokenizeBinary(start);
            case 'o':
                this->NextChar();
                return this->TokenizeOctal(start);
            case 'x':
                this->NextChar();
                return this->TokenizeHex(start);
            default:
                peeked = this->PeekChar();
                if (!IsDigit(peeked) && peeked != '.') {
                    auto zero = (unsigned char *) argon::memory::Alloc(2);

                    if (zero == nullptr) {
                        this->status = ScannerStatus::NOMEM;
                        return Token(TokenType::ERROR, start, this->pos_);
                    }

                    zero[0] = '0';
                    zero[1] = '\0';

                    return Token(TokenType::NUMBER, start, this->pos_, zero);
                }
        }
    }

    return this->TokenizeDecimal(start, begin_zero);
}

Token Scanner::TokenizeRawString(Pos start) {
    int hashes = 0;
    int count = 0;
    int value;

    // Count beginning hashes
    for (; this->PeekChar() == '#'; this->NextChar(), hashes++);

    if (this->NextChar() != '"') {
        this->status = ScannerStatus::INVALID_RS_PROLOGUE;
        return Token(TokenType::ERROR, start, this->pos_);
    }

    while ((value = this->PeekChar()) > 0) {
        if (value == '"') {
            this->NextChar();
            for (; this->PeekChar() == '#'; this->NextChar(), count++);
            if (count != hashes) {
                if (!this->TkPutChar('"'))
                    goto ERROR;

                while (count > 0) {
                    if (!this->TkPutChar('#'))
                        goto ERROR;
                    count--;
                }
                continue;
            }
            return this->MakeTkWithValue(start, TokenType::RAW_STRING);
        }

        if (!this->TkPutChar())
            goto ERROR;
    }

    this->status = ScannerStatus::INVALID_RSTR;

    ERROR:
    return Token(TokenType::ERROR, start, this->pos_);
}

Token Scanner::TokenizeString(Pos start, bool byte_string) {
    TokenType type = TokenType::STRING;
    int value = this->NextChar();

    if (byte_string)
        type = TokenType::BYTE_STRING;

    while (value != '"') {
        if (value < 0 || value == '\n') {
            this->status = ScannerStatus::INVALID_STR;
            goto ERROR;
        }

        // Byte string accept byte in range (0x00 - 0x7F)
        if (byte_string && value > 0x7F) {
            this->status = ScannerStatus::INVALID_BSTR;
            goto ERROR;
        }

        if (value == '\\') {
            if (this->PeekChar() != '\\') {
                if (!this->ProcessEscape('"', byte_string))
                    goto ERROR;

                value = this->NextChar();
                continue;
            }

            value = this->NextChar();
        }

        if (!this->TkPutChar(value))
            goto ERROR;

        value = this->NextChar();
    }

    return this->MakeTkWithValue(start, type);

    ERROR:
    return Token(TokenType::ERROR, start, this->pos_);
}

Token Scanner::TokenizeWord() {
    Pos start = this->pos_;
    int value = this->NextChar();
    TokenType type = TokenType::IDENTIFIER;

    if (value == 'b' && this->PeekChar() == '"') {
        this->NextChar(); // Discard "
        return this->TokenizeString(start, true);
    }

    if (value == 'r' && (this->PeekChar() == '#' || this->PeekChar() == '"'))
        return this->TokenizeRawString(start);

    if (!this->TkPutChar(value))
        return Token(TokenType::ERROR, start, this->pos_);

    value = this->PeekChar();

    while (IsAlpha(value) || IsDigit(value)) {
        if (!this->TkPutChar())
            return Token(TokenType::ERROR, start, this->pos_);
        value = this->PeekChar();
    }

    // keywords are longer than one letter
    if ((this->pos_ - start) > 1) {
        for (KwToken kt: kw2tktype) {
            if (argon::memory::MemoryCompare(kt.keyword, this->tkval.start_, this->pos_ - start) == 0) {
                type = kt.type;
                break;
            }
        }
    }

    return this->MakeTkWithValue(start, type);
}

unsigned char *Scanner::TkGetValue() {
    unsigned char *tmp;

    if (this->tkval.cur_ == nullptr || *this->tkval.cur_ != '\0') {
        if (this->tkval.cur_ + 1 >= this->tkval.end_) {
            if (!this->TkEnlarge(1))
                return nullptr;
        }

        *this->tkval.cur_ = '\0';
        this->tkval.cur_++;
    }

    tmp = this->tkval.start_;
    this->tkval.start_ = nullptr;
    this->tkval.cur_ = nullptr;
    this->tkval.end_ = nullptr;

    return tmp;
}

// PUBLIC

Scanner::Scanner(FILE *fd, const char *ps1, const char *ps2) noexcept {
    this->fd_ = fd;
    this->prompt_ = ps1;
    this->next_prompt_ = ps2;

    this->promptfn_ = DefaultPrompt;
}

Scanner::Scanner(const char *str, unsigned long len) noexcept {
    this->buffers.start_ = (unsigned char *) str;
    this->buffers.cur_ = this->buffers.start_;
    this->buffers.inp_ = this->buffers.start_ + len;
    this->buffers.end_ = this->buffers.start_ + len;
}

Scanner::~Scanner() noexcept {
    if (this->prompt_ != nullptr)
        argon::memory::Free(this->buffers.start_);

    argon::memory::Free(this->tkval.start_);
}

bool Scanner::Reset() {
    // Buffers
    this->buffers.cur_ = this->buffers.start_;
    this->buffers.inp_ = this->buffers.start_;

    // TokenVal
    this->tkval.cur_ = this->tkval.start_;

    this->pos_ = 1;

    if (this->fd_ != nullptr)
        return fseek(this->fd_, 0, SEEK_SET) == 0;

    return true;
}

const char *Scanner::GetStatusMessage() {
    static const char *messages[] = {
            "empty '' not allowed",
            "byte string can only contain ASCII literal characters",
            "can't decode bytes in unicode sequence, escape format must be: \\Uhhhhhhhh",
            "can't decode bytes in unicode sequence, escape format must be: \\uhhhh",
            "can't decode byte, hex escape must be: \\xhh",
            "expected new-line after line continuation character",
            "unterminated string",
            "invalid raw string prologue",
            "expected '",
            "unterminated string",
            "invalid token",
            "illegal Unicode character",
            "not enough memory",
            "ok"
    };

    return messages[(int) this->status];
}

Token Scanner::NextToken() noexcept {
#define CHECK_AGAIN(chr, type)                      \
    if(this->PeekChar() == (chr))   {               \
        this->NextChar();                           \
        return Token((type), start, this->pos_);    \
    }

    int value;
    Pos start;

    while ((value = this->PeekChar()) > 0) {
        start = this->pos_;

        // Skip spaces
        if (IsSpace(value)) {
            for (; IsSpace(this->PeekChar()); this->NextChar());
            continue;
        }

        if (IsAlpha(value))
            return this->TokenizeWord();

        // Numbers
        if (IsDigit(value))
            return this->TokenizeNumber();

        this->NextChar();

        switch (value) {
            case '\n': // NewLine
                for (; this->PeekChar() == '\n'; this->NextChar());
                return Token(TokenType::END_OF_LINE, start, this->pos_);
            case '\r': // \r\n
                CHECK_AGAIN('\n', TokenType::END_OF_LINE)
                return Token(TokenType::ERROR, start, this->pos_);
            case '\\':
                if (this->NextChar() != '\n') {
                    this->status = ScannerStatus::INVALID_LC;
                    return Token(TokenType::ERROR, start, this->pos_);
                }
                continue;
            case '!':
                CHECK_AGAIN('=', TokenType::NOT_EQUAL)
                CHECK_AGAIN('{', TokenType::EXCLAMATION_LBRACES)
                return Token(TokenType::EXCLAMATION, start, this->pos_);
            case '"':
                return this->TokenizeString(start, false);
            case '#':
                return this->TokenizeComment(start, true);
            case '%':
                return Token(TokenType::PERCENT, start, this->pos_);
            case '&':
                CHECK_AGAIN('&', TokenType::AND)
                return Token(TokenType::AMPERSAND, start, this->pos_);
            case '\'':
                return this->TokenizeChar(start);
            case '(':
                return Token(TokenType::LEFT_ROUND, start, this->pos_);
            case ')':
                return Token(TokenType::RIGHT_ROUND, start, this->pos_);
            case '*':
                CHECK_AGAIN('=', TokenType::ASTERISK_EQ)
                return Token(TokenType::ASTERISK, start, this->pos_);
            case '+':
                CHECK_AGAIN('=', TokenType::PLUS_EQ)
                CHECK_AGAIN('+', TokenType::PLUS_PLUS)
                return Token(TokenType::PLUS, start, this->pos_);
            case ',':
                return Token(TokenType::COMMA, start, this->pos_);
            case '-':
                CHECK_AGAIN('=', TokenType::MINUS_EQ)
                CHECK_AGAIN('-', TokenType::MINUS_MINUS)
                return Token(TokenType::MINUS, start, this->pos_);
            case '.':
                if (this->PeekChar() == '.') {
                    this->NextChar();
                    if (this->NextChar() == '.')
                        return Token(TokenType::ELLIPSIS, start, this->pos_);

                    this->status = ScannerStatus::INVALID_TK;
                    return Token(TokenType::ERROR, start, this->pos_);
                }
                return Token(TokenType::DOT, start, this->pos_);
            case '/':
                CHECK_AGAIN('/', TokenType::SLASH_SLASH)
                CHECK_AGAIN('=', TokenType::SLASH_EQ)
                if (this->PeekChar() == '*') {
                    this->NextChar();
                    return this->TokenizeComment(start, false);
                }
                return Token(TokenType::SLASH, start, this->pos_);
            case ':':
                CHECK_AGAIN(':', TokenType::SCOPE)
                return Token(TokenType::COLON, start, this->pos_);
            case ';':
                return Token(TokenType::SEMICOLON, start, this->pos_);
            case '<':
                CHECK_AGAIN('=', TokenType::LESS_EQ)
                CHECK_AGAIN('<', TokenType::SHL)
                return Token(TokenType::LESS, start, this->pos_);
            case '=':
                CHECK_AGAIN('=', TokenType::EQUAL_EQUAL)
                CHECK_AGAIN('>', TokenType::ARROW)
                return Token(TokenType::EQUAL, start, this->pos_);
            case '>':
                CHECK_AGAIN('=', TokenType::GREATER_EQ)
                CHECK_AGAIN('>', TokenType::SHR)
                return Token(TokenType::GREATER, start, this->pos_);
            case '?':
                CHECK_AGAIN(':', TokenType::ELVIS)
                CHECK_AGAIN('<', TokenType::QUESTION_DOT)
                return Token(TokenType::QUESTION, start, this->pos_);
            case '[':
                return Token(TokenType::LEFT_SQUARE, start, this->pos_);
            case ']':
                return Token(TokenType::RIGHT_SQUARE, start, this->pos_);
            case '^':
                return Token(TokenType::CARET, start, this->pos_);
            case '{':
                return Token(TokenType::LEFT_BRACES, start, this->pos_);
            case '|':
                CHECK_AGAIN('|', TokenType::OR)
                return Token(TokenType::PIPE, start, this->pos_);
            case '}':
                return Token(TokenType::RIGHT_BRACES, start, this->pos_);
            case '~':
                return Token(TokenType::TILDE, start, this->pos_);
            default:
                this->status = ScannerStatus::INVALID_TK;
                return Token(TokenType::ERROR, start, this->pos_);
        }
    }

    return Token(TokenType::END_OF_FILE, this->pos_, this->pos_);
#undef CHECK_AGAIN
}

void Scanner::SetPromptFn(InteractiveFn fn) {
    this->promptfn_ = fn;
}