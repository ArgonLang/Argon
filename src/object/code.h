// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_CODE_H_
#define ARGON_OBJECT_CODE_H_

#include "object.h"

namespace argon::object {
    class Code : public Object {
    public:
        size_t instr_sz;
        unsigned char *instr;

        explicit Code(size_t instr_sz);

        ~Code() override;
    };
} // namespace argon::object

#endif // !ARGON_OBJECT_CODE_H_
