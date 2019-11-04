// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

// Read-Lex-Print-Loop (RLPL) utility.

#include <iostream>
#include <sstream>
#include "scanner.h"

int main(int argc, char **argv) {
    for (;;) {
        std::string source;
        std::cout << "RLPL>> ";
        for (;;) {
            std::string line;
            std::getline(std::cin, line);
            if (line.empty()) {
                if (source.empty()) {
                    std::cout << "leaving...\n";
                    return 0;
                }
                break;
            }
            source += line + "\n";
        }
        std::istringstream stream(source);
        lang::scanner::Scanner scanner(&stream);
        lang::scanner::Token tk = scanner.Next();
        do {
            std::cout << tk.String() << std::endl;
            tk = scanner.Next();
            if (tk.type == lang::scanner::TokenType::ERROR)
                break;
        } while (tk.type != lang::scanner::TokenType::END_OF_FILE);
    }
}