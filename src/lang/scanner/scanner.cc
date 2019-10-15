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
	unsigned colno = this->colno_;
	unsigned lineno = this->lineno_;

	while (this->source_.good()) {
		switch (value) {
		case 0x0A: // NewLine
			this->lineno_ += this->Skip(0xA) + 1;
			return Token(TokenType::END_OF_LINE, colno, lineno, "");
		}
		value = this->source_.get();
	}

	return this->Emit(TokenType::END_OF_FILE, "");
}