// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "scanner.h"

using namespace lang::scanner;

Token Scanner::ParseBinary(Pos start) {
    int value = this->source_->peek();
    std::string number;

    while (value >= '0' && value <= '1') {
        number += (char) value;
        this->GetCh();
        value = this->source_->peek();
    }

    return Token(TokenType::NUMBER_BIN, start, this->pos_, number);
}

Token Scanner::ParseOctal(Pos start) {
    int value = this->source_->peek();
    std::string number;

    while (value >= '0' && value <= '7') {
        number += (char) value;
        this->GetCh();
        value = this->source_->peek();
    }

    return Token(TokenType::NUMBER_OCT, start, this->pos_, number);
}

Token Scanner::ParseHex(Pos start) {
    int value = this->source_->peek();
    std::string number;

    while (IsHexDigit(value)) {
        number += (char) value;
        this->GetCh();
        value = this->source_->peek();
    }

    return Token(TokenType::NUMBER_HEX, start, this->pos_, number);
}

Token Scanner::ParseDecimal(Pos start) {
    TokenType type = TokenType::NUMBER;
    std::string number;

    for (; IsDigit(this->source_->peek()); number += (char) this->GetCh());

    // Look for a fractional part.
    if (this->source_->peek() == '.') {
        number += (char) this->GetCh();
        for (; IsDigit(this->source_->peek()); number += (char) this->GetCh());
        type = TokenType::DECIMAL;
    }

    return Token(type, start, this->pos_, number);
}

Token Scanner::ParseNumber() {
    Pos start = this->pos_;
    Token tk;

    if (this->source_->peek() == '0') {
        this->GetCh();
        switch (tolower(this->source_->peek())) {
            case 'b':
                this->GetCh();
                return this->ParseBinary(start);
            case 'o':
                this->GetCh();
                return this->ParseOctal(start);
            case 'x':
                this->GetCh();
                return this->ParseHex(start);
        }

        if (!IsDigit(this->source_->peek()))
            return Token(TokenType::NUMBER, start, this->pos_, "0");
    }

    return this->ParseDecimal(start);
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
        byte |= sequence[i] << (unsigned char) (mul * 3);
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
        byte |= HexDigitToNumber(curr) << (unsigned char) (i * 4);
    }
    return true;
}

Token Scanner::ParseString(Pos start, bool byte_string) {
    int curr = this->GetCh();
    std::string string;

    while (curr != '"') {
        if (!this->source_->good() || curr == '\n')
            return Token(TokenType::ERROR, start, this->pos_, "unterminated string");

        // Byte string accept byte in range (0x00 - 0x7F)
        if (byte_string && curr > 0x7F)
            return Token(TokenType::ERROR, start, this->pos_, "byte string can only contain ASCII literal characters");

        if (curr == '\\') {
            if (this->source_->peek() != '\\') {
                if (!this->ParseEscape('"', byte_string, string, string))
                    return Token(TokenType::ERROR, start, this->pos_, string);
                curr = this->GetCh();
                continue;
            }
            curr = this->GetCh();
        }
        string += (char) curr;
        curr = this->GetCh();
    }

    if (byte_string)
        return Token(TokenType::BYTE_STRING, start, this->pos_, string);
    return Token(TokenType::STRING, start, this->pos_, string);
}

Token Scanner::ParseRawString(Pos start) {
    std::string raw;
    int hashes = 0;
    int count = 0;

    for (; this->source_->peek() == '#'; this->GetCh(), hashes++);

    if (this->GetCh() != '"')
        return Token(TokenType::ERROR, start, this->pos_, "invalid raw string prologue");

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
            return Token(TokenType::RAW_STRING, start, this->pos_, raw);
        }
        raw += (char) this->GetCh();
    }

    return Token(TokenType::ERROR, start, this->pos_, "unterminated raw string");
}

Token Scanner::ParseWord() {
    Pos start = this->pos_;
    int value = this->GetCh();
    std::string word;

    if (value == 'b') {
        if (this->source_->peek() == '"') {
            this->GetCh();
            return this->ParseString(start, true);
        }
    }

    if (value == 'r') {
        if (this->source_->peek() == '#' || this->source_->peek() == '"')
            return this->ParseRawString(start);
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
            return Token(Keywords.at(word), start, this->pos_, word);
    }

    return Token(TokenType::IDENTIFIER, start, this->pos_, word);
}

std::string Scanner::ParseComment(bool inline_comment) {
    std::string comment;

    // Skip newline/whitespace at comment start
    for (int skip = this->source_->peek();
         IsSpace(skip) || (!inline_comment && skip == '\n');
         this->GetCh(), skip = this->source_->peek());

    while (this->source_->good()) {
        if (this->source_->peek() == '\n' && inline_comment)
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
    Pos start;
    int value;

    this->source_->peek(); // INIT

    while (this->source_->good()) {
        value = this->source_->peek();
        start = this->pos_;

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
                return Token(TokenType::END_OF_LINE, start, this->pos_, "");
            case '!':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::NOT_EQUAL, start, this->pos_, "");
                }
                if (this->source_->peek() == '.') {
                    this->GetCh();
                    return Token(TokenType::EXCLAMATION_DOT, start, this->pos_, "");
                }
                return Token(TokenType::EXCLAMATION, start, this->pos_, "");
            case '"':
                this->GetCh();
                return this->ParseString(start, false);
            case '#':
                this->GetCh();
                return Token(TokenType::INLINE_COMMENT, start, this->pos_, this->ParseComment(true));
            case '%':
                this->GetCh();
                return Token(TokenType::PERCENT, start, this->pos_, "");
            case '&':
                this->GetCh();
                if (this->source_->peek() == '&') {
                    this->GetCh();
                    return Token(TokenType::AND, start, this->pos_, "");
                }
                return Token(TokenType::AMPERSAND, start, this->pos_, "");
            case '\'':
                this->GetCh();
                break;
            case '(':
                this->GetCh();
                return Token(TokenType::LEFT_ROUND, start, this->pos_, "");
            case ')':
                this->GetCh();
                return Token(TokenType::RIGHT_ROUND, start, this->pos_, "");
            case '*':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::ASTERISK_EQ, start, this->pos_, "");
                }
                return Token(TokenType::ASTERISK, start, this->pos_, "");
            case '+':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::PLUS_EQ, start, this->pos_, "");
                }
                if (this->source_->peek() == '+') {
                    this->GetCh();
                    return Token(TokenType::PLUS_PLUS, start, this->pos_, "");
                }
                return Token(TokenType::PLUS, start, this->pos_, "");
            case ',':
                this->GetCh();
                return Token(TokenType::COMMA, start, this->pos_, "");
            case '-':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::MINUS_EQ, start, this->pos_, "");
                }
                if (this->source_->peek() == '-') {
                    this->GetCh();
                    return Token(TokenType::MINUS_MINUS, start, this->pos_, "");
                }
                return Token(TokenType::MINUS, start, this->pos_, "");
            case '.':
                this->GetCh();
                if (this->source_->peek() == '.') {
                    this->GetCh();
                    if (this->source_->peek() == '.') {
                        this->GetCh();
                        return Token(TokenType::ELLIPSIS, start, this->pos_, "");
                    }
                    this->source_->seekg(this->source_->tellg().operator-(1));
                    this->pos_--;
                }
                return Token(TokenType::DOT, start, this->pos_, "");
            case '/':
                this->GetCh();
                if (this->source_->peek() == '/') {
                    this->GetCh();
                    return Token(TokenType::SLASH_SLASH, start, this->pos_, "");
                }
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::SLASH_EQ, start, this->pos_, "");
                }
                if (this->source_->peek() == '*') {
                    this->GetCh();
                    return Token(TokenType::COMMENT, start, this->pos_, this->ParseComment(false));
                }
                return Token(TokenType::SLASH, start, this->pos_, "");
            case ':':
                this->GetCh();
                if (this->source_->peek() == ':') {
                    this->GetCh();
                    return Token(TokenType::SCOPE, start, this->pos_, "");
                }
                return Token(TokenType::COLON, start, this->pos_, "");
            case ';':
                this->GetCh();
                return Token(TokenType::SEMICOLON, start, this->pos_, "");
            case '<':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::LESS_EQ, start, this->pos_, "");
                }
                if (this->source_->peek() == '<') {
                    this->GetCh();
                    return Token(TokenType::SHL, start, this->pos_, "");
                }
                return Token(TokenType::LESS, start, this->pos_, "");
            case '=':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::EQUAL_EQUAL, start, this->pos_, "");
                }
                if (this->source_->peek() == '>') {
                    this->GetCh();
                    return Token(TokenType::ARROW, start, this->pos_, "");
                }
                return Token(TokenType::EQUAL, start, this->pos_, "");
            case '>':
                this->GetCh();
                if (this->source_->peek() == '=') {
                    this->GetCh();
                    return Token(TokenType::GREATER_EQ, start, this->pos_, "");
                }
                if (this->source_->peek() == '>') {
                    this->GetCh();
                    return Token(TokenType::SHR, start, this->pos_, "");
                }
                return Token(TokenType::GREATER, start, this->pos_, "");
            case '?':
                this->GetCh();
                if (this->source_->peek() == ':') {
                    this->GetCh();
                    return Token(TokenType::ELVIS, start, this->pos_, "");
                }
                if (this->source_->peek() == '.') {
                    this->GetCh();
                    return Token(TokenType::QUESTION_DOT, start, this->pos_, "");
                }
                return Token(TokenType::QUESTION, start, this->pos_, "");
            case '[':
                this->GetCh();
                return Token(TokenType::LEFT_SQUARE, start, this->pos_, "");
            case ']':
                this->GetCh();
                return Token(TokenType::RIGHT_SQUARE, start, this->pos_, "");
            case '^':
                this->GetCh();
                return Token(TokenType::CARET, start, this->pos_, "");
            case '{':
                this->GetCh();
                return Token(TokenType::LEFT_BRACES, start, this->pos_, "");
            case '|':
                this->GetCh();
                if (this->source_->peek() == '|') {
                    this->GetCh();
                    return Token(TokenType::OR, start, this->pos_, "");
                }
                return Token(TokenType::PIPE, start, this->pos_, "");
            case '}':
                this->GetCh();
                return Token(TokenType::RIGHT_BRACES, start, this->pos_, "");
            case '~':
                this->GetCh();
                return Token(TokenType::TILDE, start, this->pos_, "");
            default:
                return Token(TokenType::ERROR, start, this->pos_, "invalid token");
        }
    }

    return Token(TokenType::END_OF_FILE, this->pos_, this->pos_, "");
}

int Scanner::GetCh() {
    int value = this->source_->get();
    if (!this->source_->eof()) {
        this->pos_ = (Pos) this->source_->tellg();
        this->pos_++;
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