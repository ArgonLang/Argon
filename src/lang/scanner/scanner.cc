// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "scanner.h"

using namespace lang::scanner;

inline Token Scanner::Emit(TokenType type, const std::string& value){
	return this->Emit(type, this->lineno_, value);
}

inline Token Scanner::Emit(TokenType type, unsigned lineno, const std::string& value) {
	return Token(type, this->colno_, lineno, value);
}

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
		number += value;
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
		number += value;
		this->GetCh();
		value = this->source_->peek();
	}

	return Token(TokenType::NUMBER_HEX, colno, this->lineno_, number);
}

Token Scanner::ParseDecimal() {
	TokenType type = TokenType::NUMBER;
	std::string number;
	int colno = this->colno_;

	for (; IsDigit(this->source_->peek()); number += this->GetCh());

	// Look for a fractional part.
	if (this->source_->peek() == '.') {
		number += this->GetCh();
		for (; IsDigit(this->source_->peek()); number += this->GetCh());
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

bool Scanner::ParseEscape(int stopChr, std::string& dest, std::string& error) {
	int op = this->GetCh();

	if (op == stopChr) {
		dest += stopChr;
		return true;
	}

	switch (op) {
	case '\\':
		dest += '\\';
		break;
	case 'a':
		dest += (char)0x07;
		break;
	case 'b':
		dest += (char)0x08;
		break;
	case 'f':
		dest += (char)0x0C;
		break;
	case 'n':
		dest += (char)0x0A;
		break;
	case 'r':
		dest += (char)0x0D;
		break;
	case 't':
		dest += (char)0x09;
		break;
	case 'v':
		dest += (char)0x0B;
		break;
	case 'u':
		return this->ParseUnicodeEscape(dest, error, false);
	case 'U':
		return this->ParseUnicodeEscape(dest, error, true);
	case 'x':
		return this->ParseHexEscape(dest, error);
	default:
		if (!this->ParseOctEscape(dest, error, op))
			dest += "\\\\" + op;
	}
	return true;
}

bool Scanner::ParseUnicodeEscape(std::string& dest, std::string& error, bool extended) {
	unsigned char sequence[] = { 0,0,0,0 };
	unsigned int* sq_ptr = (unsigned int*)sequence;
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
	}
	else if (*sq_ptr < 0x010000) {
		dest += (*sq_ptr) >> 12 & 0x0F | 0xE0;
		dest += (*sq_ptr) >> 6 & 0x3F | 0x80;
		dest += (*sq_ptr) >> 0 & 0x3F | 0x80;
	}
	else if (*sq_ptr < 0x110000) {
		dest += (*sq_ptr) >> 18 & 0x07 | 0xF0;
		dest += (*sq_ptr) >> 12 & 0x3F | 0x80;
		dest += (*sq_ptr) >> 6 & 0x3F | 0x80;
		dest += (*sq_ptr) >> 0 & 0x3F | 0x80;
	}
	else {
		error = "illegal Unicode character";
		return false;
	}

	return true;
}

bool Scanner::ParseOctEscape(std::string& dest, std::string& error, int value) {
	unsigned char sequence[] = { 0,0,0 };
	unsigned char byte = 0;
	unsigned char curr;

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

bool Scanner::ParseHexEscape(std::string& dest, std::string& error){
	unsigned char byte;

	if (!this->ParseHexToByte(byte)) {
		error = "can't decode byte, hex escape must be: \\xhh";
		return false;
	}

	dest += byte;

	return true;
}

bool Scanner::ParseHexToByte(unsigned char& byte) {
	int curr;

	byte = 0;

	for (int i = 1; i >= 0; i--) {
		if (!IsHexDigit(curr = this->GetCh()))	
			return false;
		byte |= HexDigitToNumber(curr) << (i * 4);
	}
	return true;
}

Token Scanner::ParseString() {
	std::string string;
	int start = this->source_->tellg();
	int colno = this->colno_;
	int curr = this->GetCh();

	while (curr != '"') {
		if (!this->source_->good() || curr == '\n')
			return Token(TokenType::ERROR, colno, this->lineno_, "unterminated string");
		if (curr == '\\') {
			if (this->source_->peek() != '\\') {
				if(!this->ParseEscape('"', string, string))
					return Token(TokenType::ERROR, colno, this->lineno_, string);
				curr = this->GetCh();
				continue;
			}
			curr = this->GetCh();
		}
		string += curr;
		curr = this->GetCh();
	}

	curr = this->GetCh();

	return Token(TokenType::STRING, colno, this->lineno_, string);
}

Token Scanner::ParseWord() {
	std::string word;
	int colno = this->colno_;
	int value = this->source_->peek();

	while (IsAlpha(value)||IsDigit(value)) {
		word += value;
		this->GetCh();
		value = this->source_->peek();
	}

	return Token(TokenType::WORD, colno, this->lineno_, word);
}

int Scanner::Skip(unsigned char byte) {
	int times = 0;

	while (this->source_->peek() == byte) {
		this->GetCh();
		times++;
	}

	return times;
}

int Scanner::GetCh() {
	this->colno_++;
	return this->source_->get();
}

Token Scanner::NextToken() {
	int value;
	unsigned colno = 0;
	unsigned lineno = 0;

	while (this->source_->good()) {
		value= this->source_->peek();
		colno = this->colno_;
		lineno = this->lineno_;

		if (IsSpace(value)) {
			for (; IsSpace(this->source_->peek()); this->GetCh())
				continue;
		} // Skip spaces

		if (IsAlpha(value))
			return this->ParseWord();

		if (IsDigit(value))
			return this->ParseNumber();

		switch (value) {
		case 0x0A: // NewLine
			this->lineno_ += this->Skip(0xA);
			this->colno_ = 0;
			return Token(TokenType::END_OF_LINE, colno, lineno, "");
		case '!':
			this->GetCh();
			if (this->source_->peek() == '=') {
				this->GetCh();
				return Token(TokenType::NOT_EQUAL, colno, lineno, "");
			}
			return Token(TokenType::EXCLAMATION, colno, lineno, "");
		case '"':
			this->GetCh();
			return this->ParseString();
		case '#':
			this->GetCh();
			return Token(TokenType::HASH, colno, lineno, "");
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
			return Token(TokenType::ASTERISK, colno, lineno, "");
		case '+':
			this->GetCh();
			return Token(TokenType::PLUS, colno, lineno, "");
		case ',':
			this->GetCh();
			return Token(TokenType::COMMA, colno, lineno, "");
		case '-':
			this->GetCh();
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
			return Token(TokenType::FRACTION_SLASH, colno, lineno, "");
		case ':':
			this->GetCh();
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
			return Token(TokenType::LESS, colno, lineno, "");
		case '=':
			this->GetCh();
			return Token(TokenType::EQUAL, colno, lineno, "");
		case '>':
			this->GetCh();
			if (this->source_->peek() == '=') {
				this->GetCh();
				return Token(TokenType::GREATER_EQ, colno, lineno, "");
			}
			return Token(TokenType::GREATER, colno, lineno, "");
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
		}
	}

	return this->Emit(TokenType::END_OF_FILE, "");
}