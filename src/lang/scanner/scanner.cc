// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "scanner.h"

using namespace lang::scanner;

Token Scanner::ParseBinary() {
    std::string number;
    int colno = this->colno_;
    int value = this->source_->peek();

    while (value >= '0' && value <= '1') {
        number += value;
        this->GetCh();
        value = this->source_->peek();
    }

    return Token(TokenType::NUMBER_BIN, colno, this->lineno_, number);
}

Token Scanner::ParseOctal() {
    std::string number;
    int colno = this->colno_;
    int value = this->source_->peek();

    while (value >= '0' && value <= '7') {
        number += (char) value;
        this->GetCh();
        value = this->source_->peek();
    }

    return Token(TokenType::NUMBER_OCT, colno, this->lineno_, number);
}

Token Scanner::ParseHex() {
    std::string number;
    int colno = this->colno_;
    int value = this->source_->peek();

    while (IsHexDigit(value)) {
        number += (char) value;
        this->GetCh();
        value = this->source_->peek();
    }

    return Token(TokenType::NUMBER_HEX, colno, this->lineno_, number);
}

Token Scanner::ParseDecimal() {
    TokenType type = TokenType::NUMBER;
    std::string number;
    int colno = this->colno_;

    for (; IsDigit(this->source_->peek()); number += (char) this->GetCh());

    // Look for a fractional part.
    if (this->source_->peek() == '.') {
        number += (char) this->GetCh();
        for (; IsDigit(this->source_->peek()); number += (char) this->GetCh());
        type = TokenType::DECIMAL;
    }

    return Token(type, colno, this->lineno_, number);
}

Token Scanner::ParseNumber() {
    unsigned colno = this->colno_;
    unsigned lineno = this->lineno_;
    Token tk;

    if (this->source_->peek() == '0') {
        this->GetCh();
        switch (tolower(this->source_->peek())) {
            case 'b':
                this->GetCh();
                tk = this->ParseBinary();
                tk.colno = colno;
                return tk;
            case 'o':
                this->GetCh();
                tk = this->ParseOctal();
                tk.colno = colno;
                return tk;
            case 'x':
                this->GetCh();
                tk = this->ParseHex();
                tk.colno = colno;
                return tk;
        }

        if (!IsDigit(this->source_->peek()))
            return Token(TokenType::NUMBER, colno, lineno, "0");
    }

    tk = this->ParseDecimal();
    tk.colno = colno;
    return tk;
}

bool Scanner::ParseEscape(int stopChr, bool ignore_unicode_escape, std::string &dest, std::string &error) {
    int op = this->GetCh();

    if (op == stopChr) {
        dest += (char) stopChr;
        return true;
    }

    if (!ignore_unicode_escape) {
        if (op == 'u')
            return this->ParseUnicodeEscape(dest, error, false);
        else if (op == 'U')
            return this->ParseUnicodeEscape(dest, error, true);
    }

    switch (op) {
        case 'a':
            dest += (char) 0x07;
            break;
        case 'b':
            dest += (char) 0x08;
            break;
        case 'f':
            dest += (char) 0x0C;
            break;
        case 'n':
            dest += (char) 0x0A;
            break;
        case 'r':
            dest += (char) 0x0D;
            break;
        case 't':
            dest += (char) 0x09;
            break;
        case 'v':
            dest += (char) 0x0B;
            break;
        case 'x':
            return this->ParseHexEscape(dest, error);
        default:
            if (!this->ParseOctEscape(dest, error, op)) {
                dest += '\\';
                dest += (char) op;
            }
    }
    return true;
}

bool Scanner::ParseUnicodeEscape(std::string &dest, std::string &error, bool extended) {
    unsigned char sequence[] = {0, 0, 0, 0};
    unsigned int *sq_ptr = (unsigned int *) sequence;
    unsigned char byte;
    int width = 2;

    if (extended)
        width = 4;

    for (int i = 0; i < width; i++) {
        if (!this->ParseHexToByte(byte)) {
            if (!extended)
                error = "can't decode bytes in unicode sequence, escape format must be: \\uhhhh";
            else
                error = "can't decode bytes in unicode sequence, escape format must be: \\Uhhhhhhhh";
            return false;
        }
        sequence[(width - 1) - i] = byte;
    }

    if (*sq_ptr < 0x80)
        dest += (*sq_ptr) >> 0 & 0x7F;
    else if (*sq_ptr < 0x0800) {
        dest += (*sq_ptr) >> 6 & 0x1F | 0xC0;
        dest += (*sq_ptr) >> 0 & 0xBF;
    } else if (*sq_ptr < 0x010000) {
        dest += (*sq_ptr) >> 12 & 0x0F | 0xE0;
        dest += (*sq_ptr) >> 6 & 0x3F | 0x80;
        dest += (*sq_ptr) >> 0 & 0x3F | 0x80;
    } else if (*sq_ptr < 0x110000) {
        dest += (*sq_ptr) >> 18 & 0x07 | 0xF0;
        dest += (*sq_ptr) >> 12 & 0x3F | 0x80;
        dest += (*sq_ptr) >> 6 & 0x3F | 0x80;
        dest += (*sq_ptr) >> 0 & 0x3F | 0x80;
    } else {
        error = "illegal Unicode character";
        return false;
    }

    return true;
}

bool Scanner::ParseOctEscape(std::string &dest, std::string &error, int value) {
    unsigned char sequence[] = {0, 0, 0};
    unsigned char byte = 0;

    if (!IsOctDigit(value))
        return false;

    sequence[2] = HexDigitToNumber(value);

    for (int i = 1; i >= 0 && IsOctDigit(this->source_->peek()); i--)
        sequence[i] = HexDigitToNumber(this->GetCh());

    for (int i = 0, mul = 0; i < 3; i++) {
        byte |= sequence[i] << (mul * 3);
        if (sequence[i] != 0)
            mul++;
    }

    dest += byte;
    return true;
}

bool Scanner::ParseHexEscape(std::string &dest, std::string &error) {
    unsigned char byte;

    if (!this->ParseHexToByte(byte)) {
        error = "can't decode byte, hex escape must be: \\xhh";
        return false;
    }

    dest += byte;

    return true;
}

bool Scanner::ParseHexToByte(unsigned char &byte) {
    int curr;

    byte = 0;

    for (int i = 1; i >= 0; i--) {
        if (!IsHexDigit(curr = this->GetCh()))
            return false;
        byte |= HexDigitToNumber(curr) << (i * 4);
    }
    return true;
}

Token Scanner::ParseString(int colno, bool byte_string) {
    std::string string;
    int curr = this->GetCh();

    while (curr != '"') {
        if (!this->source_->good() || curr == '\n')
            return Token(TokenType::ERROR, colno, this->lineno_, "unterminated string");

        // Byte string accept byte in range (0x00 - 0x7F)
        if (byte_string && curr > 0x7F)
            return Token(TokenType::ERROR, colno, this->lineno_,
                         "byte string can only contain ASCII literal characters");

        if (curr == '\\') {
            if (this->source_->peek() != '\\') {
                if (!this->ParseEscape('"', byte_string, string, string))
                    return Token(TokenType::ERROR, colno, this->lineno_, string);
                curr = this->GetCh();
                continue;
            }
            curr = this->GetCh();
        }
        string += (char) curr;
        curr = this->GetCh();
    }

    if (byte_string)
        return Token(TokenType::BYTE_STRING, colno, this->lineno_, string);
    return Token(TokenType::STRING, colno, this->lineno_, string);
}

Token Scanner::ParseRawString(int colno, int lineno) {
    std::string raw;
    int hashes = 0;
    int count = 0;

    for (; this->source_->peek() == '#'; this->GetCh(), hashes++);

    if (this->GetCh() != '"')
        return Token(TokenType::ERROR, colno, lineno, "invalid raw string prologue");

    while (this->source_->good()) {
        if (this->source_->peek() == '"') {
            this->GetCh();
            for (; this->source_->peek() == '#' && count != hashes; this->GetCh(), count++);
            if (count != hashes) {
                raw += '"';
                while (count > 0) {
                    raw += '#';
                    count--;
                }
                continue;
            }
            return Token(TokenType::RAW_STRING, colno, lineno, raw);
        }
        raw += (char) this->GetCh();
    }

    return Token(TokenType::ERROR, colno, lineno, "unterminated raw string");
}

Token Scanner::ParseWord() {
    std::string word;
    int colno = this->colno_;
    int value = this->GetCh();

    if (value == 'b') {
        if (this->source_->peek() == '"') {
            this->GetCh();
            return this->ParseString(colno, true);
        }
    }

    if (value == 'r') {
        if (this->source_->peek() == '#' || this->source_->peek() == '"')
            return this->ParseRawString(colno, this->lineno_);
    }

    word += (char) value;
    value = this->source_->peek();

    while (IsAlpha(value) || IsDigit(value)) {
        word += (char) value;
        this->GetCh();
        value = this->source_->peek();
    }

    // keywords are longer than one letter
    if (word.size() > 1) {
        if (Keywords.find(word) != Keywords.end())
            return Token(Keywords.at(word), colno, this->lineno_, "");
    }

    return Token(TokenType::IDENTIFIER, colno, this->lineno_, word);
}

std::string Scanner::ParseComment(bool inline_coment) {
    std::string comment;

    // Skip newline/whitespace at comment start
    for (int skip = this->source_->peek();
         IsSpace(skip) || (!inline_coment && skip == '\n');
         this->GetCh(), skip = this->source_->peek());

    while (this->source_->good()) {
        if (this->source_->peek() == '\n' && inline_coment)
            break;

        if (this->source_->peek() == '*') {
            this->GetCh();
            if (this->source_->peek() == '/')
                break;
            comment += '*';
            continue;
        }
        comment += (char) this->GetCh();
    }
    this->GetCh();
    return comment;
}

Token Scanner::NextToken() {
    int value = this->source_->peek();
    int colno = 0;
    int lineno = 0;

    while (this->source_->good()) {
        value = this->source_->peek();
        colno = this->colno_;
        lineno = this->lineno_;

        if (IsSpace(value)) {
            for (; IsSpace(this->source_->peek()); this->GetCh());
            continue;
        } // Skip spaces

        if (IsAlpha(value))
            return this->ParseWord();

        if (IsDigit(value))
            return this->ParseNumber();

        switch (value) {
            case 0x0A: // NewLine
                for (; this->source_->peek() == '\n'; this->GetCh());
                return Token(TokenType::END_OF_LINE, colno, lineno, "");
            case '!':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::NOT_EQUAL, colno, lineno, "");
                }
                if (this->source_->peek() == '.') {
                    this->GetCh();
                    return Token(TokenType::EXCLAMATION_DOT, colno, lineno, "");
                }
                return Token(TokenType::EXCLAMATION, colno, lineno, "");
            case '"':
                this->GetCh();
                return this->ParseString(colno, false);
            case '#':
                this->GetCh();
                return Token(TokenType::INLINE_COMMENT, colno, lineno, this->ParseComment(true));
            case '%':
                this->GetCh();
                return Token(TokenType::PERCENT, colno, lineno, "");
            case '&':
                this->GetCh();
                if (this->source_->peek() == '&') {
                    this->GetCh();
                    return Token(TokenType::AND, colno, lineno, "");
                }
                return Token(TokenType::AMPERSAND, colno, lineno, "");
            case '\'':
                this->GetCh();
                break;
            case '(':
                this->GetCh();
                return Token(TokenType::LEFT_ROUND, colno, lineno, "");
            case ')':
                this->GetCh();
                return Token(TokenType::RIGHT_ROUND, colno, lineno, "");
            case '*':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::ASTERISK_EQ, colno, lineno, "");
                }
                return Token(TokenType::ASTERISK, colno, lineno, "");
            case '+':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::PLUS_EQ, colno, lineno, "");
                }
                if (this->source_->peek() == '+') {
                    this->GetCh();
                    return Token(TokenType::PLUS_PLUS, colno, lineno, "");
                }
                return Token(TokenType::PLUS, colno, lineno, "");
            case ',':
                this->GetCh();
                return Token(TokenType::COMMA, colno, lineno, "");
            case '-':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::MINUS_EQ, colno, lineno, "");
                }
                if (this->source_->peek() == '-') {
                    this->GetCh();
                    return Token(TokenType::MINUS_MINUS, colno, lineno, "");
                }
                return Token(TokenType::MINUS, colno, lineno, "");
            case '.':
                this->GetCh();
                if (this->source_->peek() == '.') {
                    this->GetCh();
                    if (this->source_->peek() == '.') {
                        this->GetCh();
                        return Token(TokenType::ELLIPSIS, colno, lineno, "");
                    }
                    this->source_->seekg(this->source_->tellg().operator-(1));
                    this->colno_--;
                }
                return Token(TokenType::DOT, colno, lineno, "");
            case '/':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::SLASH_EQ, colno, lineno, "");
                }
                if (this->source_->peek() == '*') {
                    this->GetCh();
                    return Token(TokenType::COMMENT, colno, lineno, this->ParseComment(false));
                }
                return Token(TokenType::FRACTION_SLASH, colno, lineno, "");
            case ':':
                this->GetCh();
                if (this->source_->peek() == ':') {
                    this->GetCh();
                    return Token(TokenType::SCOPE, colno, lineno, "");
                }
                return Token(TokenType::COLON, colno, lineno, "");
            case ';':
                this->GetCh();
                return Token(TokenType::SEMICOLON, colno, lineno, "");
            case '<':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::LESS_EQ, colno, lineno, "");
                }
                if (this->source_->peek() == '<') {
                    this->GetCh();
                    return Token(TokenType::SHL, colno, lineno, "");
                }
                return Token(TokenType::LESS, colno, lineno, "");
            case '=':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::EQUAL_EQUAL, colno, lineno, "");
                }
                return Token(TokenType::EQUAL, colno, lineno, "");
            case '>':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::GREATER_EQ, colno, lineno, "");
                }
                if (this->source_->peek() == '>') {
                    this->GetCh();
                    return Token(TokenType::SHR, colno, lineno, "");
                }
                return Token(TokenType::GREATER, colno, lineno, "");
            case '?':
                this->GetCh();
                if (this->source_->peek() == '.') {
                    this->GetCh();
                    return Token(TokenType::QUESTION_DOT, colno, lineno, "");
                }
                return Token(TokenType::QUESTION, colno, lineno, "");
            case '[':
                this->GetCh();
                return Token(TokenType::LEFT_SQUARE, colno, lineno, "");
            case ']':
                this->GetCh();
                return Token(TokenType::RIGHT_SQUARE, colno, lineno, "");
            case '^':
                this->GetCh();
                return Token(TokenType::CARET, colno, lineno, "");
            case '{':
                this->GetCh();
                return Token(TokenType::LEFT_BRACES, colno, lineno, "");
            case '|':
                this->GetCh();
                if (this->source_->peek() == '|') {
                    this->GetCh();
                    return Token(TokenType::OR, colno, lineno, "");
                }
                return Token(TokenType::PIPE, colno, lineno, "");
            case '}':
                this->GetCh();
                return Token(TokenType::RIGHT_BRACES, colno, lineno, "");
            case '~':
                this->GetCh();
                return Token(TokenType::TILDE, colno, lineno, "");
            default:
                return Token(TokenType::ERROR, colno, lineno, "invalid token");
        }
    }

    return Token(TokenType::END_OF_FILE, this->colno_, this->lineno_, "");
}

int Scanner::GetCh() {
    int value = this->source_->get();
    this->colno_++;
    if (value == '\n') {
        this->colno_ = 0;
        this->lineno_++;
    }
    return value;
}

Token Scanner::Peek() {
    if (!this->peeked_) {
        this->peeked_token_ = this->NextToken();
        this->peeked_ = true;
        return this->peeked_token_;
    }

    return this->peeked_token_;
}

Token Scanner::Next() {
    if (this->peeked_) {
        this->peeked_ = false;
        return this->peeked_token_;
    }

    return this->NextToken();
}