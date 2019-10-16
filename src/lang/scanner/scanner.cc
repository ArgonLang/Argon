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
	int value = this->source_.peek();

	while (value >= '0' && value <= '1') {
		number += value;
		this->GetCh();
		value = this->source_.peek();
	}

	return Token(TokenType::NUMBER_BIN, colno, this->lineno_, number);
}

Token Scanner::ParseOctal() {
	std::string number;
	int colno = this->colno_;
	int value = this->source_.peek();

	while (value >= '0' && value <= '7') {
		number += value;
		this->GetCh();
		value = this->source_.peek();
	}

	return Token(TokenType::NUMBER_OCT, colno, this->lineno_, number);
}

Token Scanner::ParseHex() {
	std::string number;
	int colno = this->colno_;
	int value = this->source_.peek();

	while (IsHexDigit(value)) {
		number += value;
		this->GetCh();
		value = this->source_.peek();
	}

	return Token(TokenType::NUMBER_HEX, colno, this->lineno_, number);
}

Token Scanner::ParseDecimal() {
	TokenType type = TokenType::NUMBER;
	std::string number;
	int colno = this->colno_;

	for (; IsDigit(this->source_.peek()); number += this->GetCh());

	// Look for a fractional part.
	if (this->source_.peek() == '.') {
		number += this->GetCh();
		for (; IsDigit(this->source_.peek()); number += this->GetCh());
		type = TokenType::DECIMAL;
	}

	return Token(type, colno, this->lineno_, number);
}

Token Scanner::ParseNumber() {
	unsigned colno = this->colno_;
	unsigned lineno = this->lineno_;
	Token tk;

	if (this->source_.peek() == '0') {
		this->GetCh();
		switch (tolower(this->source_.peek())) {
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

		if (!IsDigit(this->source_.peek()))
			return Token(TokenType::NUMBER, colno, lineno, "0");
	}

	tk = this->ParseDecimal();
	tk.colno = colno;
	return tk;
}

Token lang::scanner::Scanner::ParseWord() {
	std::string word;
	int colno = this->colno_;
	int value = this->source_.peek();

	while (IsAlpha(value)||IsDigit(value)) {
		word += value;
		this->GetCh();
		value = this->source_.peek();
	}

	return Token(TokenType::WORD, colno, this->lineno_, word);
}

int Scanner::Skip(unsigned char byte) {
	int times = 0;

	while (this->source_.peek() == byte) {
		this->GetCh();
		times++;
	}

	return times;
}

int Scanner::GetCh() {
	this->colno_++;
	return this->source_.get();
}

Token Scanner::NextToken() {
	int value;
	unsigned colno = 0;
	unsigned lineno = 0;

	while (this->source_.good()) {
		value= this->source_.peek();
		colno = this->colno_;
		lineno = this->lineno_;

		if (IsSpace(value)) {
			for (; IsSpace(this->source_.peek()); this->GetCh())
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
		}
	}

	return this->Emit(TokenType::END_OF_FILE, "");
}