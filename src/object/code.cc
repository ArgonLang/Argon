// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include "code.h"

using namespace argon::object;

Code::Code(size_t instr_sz) : instr_sz(instr_sz) {
    this->instr = (unsigned char *) argon::memory::Alloc(instr_sz);
}

Code::~Code() { argon::memory::Free(this->instr); }
