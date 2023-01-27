// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cctype>

#include <vm/datatype/stringbuilder.h>
#include <vm/memory/memory.h>

#include "scanner.h"

using namespace argon::lang::scanner;

struct KwToken {
    const char *keyword;
    TokenType type;
};

constexpr KwToken kw2tktype[] = {
        {"as",          TokenType::KW_AS},
        {"async",       TokenType::KW_ASYNC},
        {"assert",      TokenType::KW_ASSERT},
        {"await",       TokenType::KW_AWAIT},
        {"break",       TokenType::KW_BREAK},
        {"case",        TokenType::KW_CASE},
        {"continue",    TokenType::KW_CONTINUE},
        {"default",     TokenType::KW_DEFAULT},
        {"defer",       TokenType::KW_DEFER},
        {"elif",        TokenType::KW_ELIF},
        {"else",        TokenType::KW_ELSE},
        {"fallthrough", TokenType::KW_FALLTHROUGH},
        {"false",       TokenType::FALSE},
        {"for",         TokenType::KW_FOR},
        {"from",        TokenType::KW_FROM},
        {"func",        TokenType::KW_FUNC},
        {"if",          TokenType::KW_IF},
        {"in",          TokenType::KW_IN},
        {"impl",        TokenType::KW_IMPL},
        {"import",      TokenType::KW_IMPORT},
        {"let",         TokenType::KW_LET},
        {"loop",        TokenType::KW_LOOP},
        {"nil",         TokenType::NIL},
        {"panic",       TokenType::KW_PANIC},
        {"pub",         TokenType::KW_PUB},
        {"return",      TokenType::KW_RETURN},
        {"self",        TokenType::SELF},
        {"spawn",       TokenType::KW_SPAWN},
        {"struct",      TokenType::KW_STRUCT},
        {"switch",      TokenType::KW_SWITCH},
        {"trait",       TokenType::KW_TRAIT},
        {"trap",        TokenType::KW_TRAP},
        {"true",        TokenType::TRUE},
        {"var",         TokenType::KW_VAR},
        {"yield",       TokenType::KW_YIELD},
        {"weak",        TokenType::KW_WEAK}
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

inline bool IsHexDigit(int chr) { return (chr >= '0' && chr <= '9') || (tolower(chr) >= 'a' && tolower(chr) <= 'f'); }

inline bool IsOctDigit(int chr) { return chr >= '0' && chr <= '7'; }

int Scanner::HexToByte() {
    int byte = 0;
    int curr;

    for (int i = 1; i >= 0; i--) {
        curr = this->Next();

        if (!IsHexDigit(curr))
            return -1;

        byte |= HexDigitToNumber(curr) << (unsigned char) (i * 4);
    }

    return byte;
}

bool Scanner::ParseEscape(int stop, bool ignore_unicode) {
    int value = this->Next();
    bool ok;

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

bool Scanner::TokenizeAtom(Token *out_token) {
    int value = this->Peek();

    while (isalnum(value) || value == '_') {
        if (!this->sbuf_.PutChar((unsigned char) this->Next())) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }

        value = this->Peek();
    }

    out_token->type = TokenType::ATOM;
    out_token->loc.end = this->loc;
    out_token->length = this->sbuf_.GetBuffer(&out_token->buffer);
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

bool Scanner::TokenizeChar(Token *out_token) {
    int value = this->Peek();

    if (value == '\'') {
        this->status_ = ScannerStatus::EMPTY_SQUOTE;
        return false;
    }

    if (value == '\\') {
        this->Next();

        if (this->Peek() != '\\') {
            if (!this->ParseEscape('\'', false))
                return false;
        } else {
            if (!this->sbuf_.PutChar((unsigned char) this->Next())) {
                this->status_ = ScannerStatus::NOMEM;
                return false;
            }
        }
    } else {
        if (!this->sbuf_.PutChar((unsigned char) this->Next())) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }
    }

    if (this->Next() != '\'') {
        this->status_ = ScannerStatus::INVALID_SQUOTE;
        return false;
    }

    out_token->type = TokenType::NUMBER_CHR;
    out_token->loc.end = this->loc;
    out_token->length = this->sbuf_.GetBuffer(&out_token->buffer);
    return true;
}

bool Scanner::TokenizeComment(Token *out_token, bool inline_comment) {
    TokenType type = TokenType::COMMENT;
    int peek;

    if (inline_comment)
        type = TokenType::COMMENT_INLINE;

    // Skip newline/whitespace at comment start
    for (int skip = this->Peek();
         isspace(skip) || (!inline_comment && skip == '\n');
         this->Next(), skip = this->Peek());

    peek = this->Peek();
    while (peek > 0 && (peek != '\n' || !inline_comment)) {
        peek = this->Next();

        if (!inline_comment && peek == '*' && this->Peek() == '/')
            break;

        if (!this->sbuf_.PutChar((unsigned char) peek)) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }

        peek = this->Peek();
    }

    if (peek == 0)
        return false;

    out_token->type = type;
    out_token->loc.end = this->loc;
    out_token->length = this->sbuf_.GetBuffer(&out_token->buffer);

    this->Next();
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

    while (IsHexDigit(value)) {
        if (!this->sbuf_.PutChar((unsigned char) this->Next())) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }

        value = this->Peek();
    }

    if (isalpha(value) && (value != 'u' && value != 'U')) {
        this->status_ = ScannerStatus::INVALID_HEX_LITERAL;
        return false;
    }

    out_token->type = TokenType::NUMBER_HEX;
    out_token->loc.end = this->loc;
    out_token->length = this->sbuf_.GetBuffer(&out_token->buffer);
    return true;
}

bool Scanner::TokenizeNumber(Token *out_token) {
    bool ok;

    if (this->Peek() == '0') {
        this->Next();

        switch (tolower(this->Peek())) {
            case 'b':
                this->Next();
                ok = this->TokenizeBinary(out_token);
                break;
            case 'o':
                this->Next();
                ok = this->TokenizeOctal(out_token);
                break;
            case 'x':
                this->Next();
                ok = this->TokenizeHex(out_token);
                break;
            default:
                ok = this->TokenizeDecimal(out_token, TokenType::NUMBER, true);
        }
    } else
        ok = this->TokenizeDecimal(out_token, TokenType::NUMBER, false);

    if (ok && (this->Peek() == 'u' || this->Peek() == 'U')) {
        this->Next();

        switch (out_token->type) {
            case TokenType::NUMBER_BIN:
                out_token->type = TokenType::U_NUMBER_BIN;
                break;
            case TokenType::NUMBER_OCT:
                out_token->type = TokenType::U_NUMBER_OCT;
                break;
            case TokenType::NUMBER_HEX:
                out_token->type = TokenType::U_NUMBER_HEX;
                break;
            case TokenType::NUMBER:
                out_token->type = TokenType::U_NUMBER;
                break;
            default:
                this->status_ = ScannerStatus::INVALID_U_NUM;
                return false;
        }
    }

    return ok;
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

    bool next = false;
    while (isalnum(value) || value == '_') {
        if (!this->sbuf_.PutChar((unsigned char) value)) {
            this->status_ = ScannerStatus::NOMEM;
            return false;
        }

        if (next)
            this->Next();

        value = this->Peek();
        next = true;
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
            if (strlen(kt.keyword) == delta &&
                argon::vm::memory::MemoryCompare(kt.keyword, out_token->buffer, delta) == 0) {
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
#define RETURN_TK(tk_type)          \
    do {                            \
    out_token->loc.end = this->loc; \
    out_token->type = (tk_type);    \
    return true; } while(false)

#define CHECK_AGAIN(chr, tk_type)   \
    if(this->Peek() == (chr)) {     \
        this->Next();               \
        RETURN_TK(tk_type); }

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
            case '\r': // \r\n
                CHECK_AGAIN('\n', TokenType::END_OF_LINE)
                this->status_ = ScannerStatus::INVALID_TK;
                return false;
            case '\\':
                if (this->Next() != '\n') {
                    this->status_ = ScannerStatus::INVALID_LC;
                    return false;
                }
                continue;
            case '!':
                if (this->Peek() == '=') {
                    this->Next();
                    CHECK_AGAIN('=', TokenType::NOT_EQUAL_STRICT)
                    RETURN_TK(TokenType::NOT_EQUAL);
                }
                RETURN_TK(TokenType::EXCLAMATION);
            case '"':
                return this->TokenizeString(out_token, false);
            case '#':
                return this->TokenizeComment(out_token, true);
            case '%':
                RETURN_TK(TokenType::PERCENT);
            case '&':
                CHECK_AGAIN('&', TokenType::AND)
                RETURN_TK(TokenType::AMPERSAND);
            case '\'':
                return this->TokenizeChar(out_token);
            case '(':
                RETURN_TK(TokenType::LEFT_ROUND);
            case ')':
                RETURN_TK(TokenType::RIGHT_ROUND);
            case '*':
                CHECK_AGAIN('=', TokenType::ASSIGN_MUL)
                RETURN_TK(TokenType::ASTERISK);
            case '+':
                CHECK_AGAIN('=', TokenType::ASSIGN_ADD)
                CHECK_AGAIN('+', TokenType::PLUS_PLUS)
                RETURN_TK(TokenType::PLUS);
            case ',':
                RETURN_TK(TokenType::COMMA);
            case '-':
                CHECK_AGAIN('=', TokenType::ASSIGN_SUB)
                CHECK_AGAIN('-', TokenType::MINUS_MINUS)
                RETURN_TK(TokenType::MINUS);
            case '.':
                if (this->Peek() == '.') {
                    this->Next();
                    CHECK_AGAIN('.', TokenType::ELLIPSIS)
                    this->status_ = ScannerStatus::INVALID_TK;
                    return false;
                }

                if (isdigit(this->Peek()))
                    return this->TokenizeDecimal(out_token, TokenType::DECIMAL, false);

                RETURN_TK(TokenType::DOT);
            case '/':
                CHECK_AGAIN('/', TokenType::SLASH_SLASH)
                CHECK_AGAIN('=', TokenType::ASSIGN_SLASH)
                if (this->Peek() == '*') {
                    this->Next();
                    return this->TokenizeComment(out_token, false);
                }
                RETURN_TK(TokenType::SLASH);
            case ':':
                CHECK_AGAIN(':', TokenType::SCOPE)
                RETURN_TK(TokenType::COLON);
            case ';':
                RETURN_TK(TokenType::SEMICOLON);
            case '<':
                CHECK_AGAIN('=', TokenType::LESS_EQ)
                CHECK_AGAIN('<', TokenType::SHL)
                RETURN_TK(TokenType::LESS);
            case '=':
                if (this->Peek() == '=') {
                    this->Next();
                    CHECK_AGAIN('=', TokenType::EQUAL_STRICT)
                    RETURN_TK(TokenType::EQUAL_EQUAL);
                }
                CHECK_AGAIN('>', TokenType::FAT_ARROW)
                RETURN_TK(TokenType::EQUAL);
            case '>':
                CHECK_AGAIN('=', TokenType::GREATER_EQ)
                CHECK_AGAIN('>', TokenType::SHR)
                RETURN_TK(TokenType::GREATER);
            case '?':
                CHECK_AGAIN(':', TokenType::ELVIS)
                CHECK_AGAIN('.', TokenType::QUESTION_DOT)
                RETURN_TK(TokenType::QUESTION);
            case '@':
                return this->TokenizeAtom(out_token);
            case '[':
                RETURN_TK(TokenType::LEFT_SQUARE);
            case ']':
                RETURN_TK(TokenType::RIGHT_SQUARE);
            case '^':
                RETURN_TK(TokenType::CARET);
            case '{':
                RETURN_TK(TokenType::LEFT_BRACES);
            case '|':
                CHECK_AGAIN('|', TokenType::OR)
                CHECK_AGAIN('>', TokenType::PIPELINE)
                RETURN_TK(TokenType::PIPE);
            case '}':
                RETURN_TK(TokenType::RIGHT_BRACES);
            case '~':
                RETURN_TK(TokenType::TILDE);
            default:
                this->status_ = ScannerStatus::INVALID_TK;
                return false;
        }
    }

    out_token->loc.start = this->loc;
    out_token->loc.end = this->loc;
    out_token->type = TokenType::END_OF_FILE;
    return true;
}

const char *Scanner::GetStatusMessage() const {
    static const char *messages[] = {
            "empty '' not allowed",
            "end of file reached",
            "invalid digit in binary literal",
            "byte string can only contain ASCII literal characters",
            "can't decode bytes in unicode sequence, escape format must be: \\Uhhhhhhhh",
            "can't decode bytes in unicode sequence, escape format must be: \\uhhhh",
            "can't decode byte, hex escape must be: \\xhh",
            "invalid hexadecimal literal",
            "expected new-line after line continuation character",
            "invalid digit in octal literal",
            "unterminated string",
            "invalid raw string prologue",
            "expected '",
            "unterminated string",
            "invalid token",
            "illegal Unicode character",
            "invalid unsigned qualifier here",
            "not enough memory",
            "ok"
    };

    return messages[(int) this->status_];
}