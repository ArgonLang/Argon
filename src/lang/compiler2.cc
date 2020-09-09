// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "parser.h"
#include "compiler2.h"

using namespace argon::lang;
using namespace argon::object;


Compiler2::Compiler2() : Compiler2(nullptr) {}

Compiler2::Compiler2(argon::object::Map *statics_globals) {
    if ((this->statics_globals = statics_globals) == nullptr) {
        this->statics_globals = MapNew();
        assert(this->statics_globals != nullptr); // TODO: fix memory error
    }
}

Compiler2::~Compiler2() {
    // ???
}

argon::object::Code *Compiler2::Compile(std::istream *source) {
    Parser parser(source);

    return nullptr;
}
