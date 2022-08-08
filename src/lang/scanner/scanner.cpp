// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/memory/memory.h>

#include "scanner.h"

using namespace argon::lang::scanner;

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

bool Scanner::TokenizeNumber(Token *out_token) {
    bool begin_zero = false;

    if (this->Peek() == '0') {
        this->Next();

        switch (this->Peek()) {
            case 'b':
            case 'o':
            case 'x':
                assert(false);
            default:
                begin_zero = true;
                break;
        }
    }

    return this->TokenizeDecimal(out_token, TokenType::NUMBER, begin_zero);
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