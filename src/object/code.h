// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_CODE_H_
#define ARGON_OBJECT_CODE_H_

#include "object.h"
#include "list.h"
#include "tuple.h"

namespace argon::object {
    struct Code : ArObject {
        argon::object::Tuple *statics;
        argon::object::Tuple *names;
        argon::object::Tuple *locals;
        argon::object::Tuple *deref;

        const unsigned char *instr;

        unsigned int instr_sz;
        unsigned int stack_sz;

        size_t hash;
    };

    Code *CodeNew(const unsigned char *instr,
                  unsigned int instr_sz,
                  unsigned int stack_sz,
                  argon::object::List *statics,
                  argon::object::List *names,
                  argon::object::List *locals,
                  argon::object::List *deref);
} // namespace argon::object

#endif // !ARGON_OBJECT_CODE_H_
