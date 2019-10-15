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

int Scanner::Skip(unsigned char byte) {
	int times = 0;

	while (this->source_.peek() == byte) {
		this->source_.ignore();
		times++;
	}

	return times;
}

Token Scanner::NextToken() {
	int value = this->source_.get();
	unsigned colno = 0;
	unsigned lineno = 0;

	while (this->source_.good()) {
		colno = this->colno_;
		lineno = this->lineno_;

		if (IsSpace(value)) {
			value = this->source_.get();
			this->colno_++;
			continue;
		} // Skip spaces

		switch (value) {
		case 0x0A: // NewLine
			this->lineno_ += this->Skip(0xA) + 1;
			this->colno_ = 0;
			return Token(TokenType::END_OF_LINE, colno, lineno, "");
		}
		value = this->source_.get();
		this->colno_++;
	}

	return this->Emit(TokenType::END_OF_FILE, "");
}