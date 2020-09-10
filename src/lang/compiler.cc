// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "lang/parser.h"
#include "compiler.h"

using namespace argon::lang;
using namespace argon::object;

Compiler::Compiler() : Compiler(nullptr) {}

Compiler::Compiler(argon::object::Map *statics_globals) {
    if ((this->statics_globals = statics_globals) == nullptr) {
        this->statics_globals = MapNew();
        assert(this->statics_globals != nullptr); // TODO: fix memory error
    }
}

Compiler::~Compiler() {
    // ???
}

argon::object::Code *Compiler::Compile(std::istream *source) {
    Parser parser(source);

    return nullptr;
}
