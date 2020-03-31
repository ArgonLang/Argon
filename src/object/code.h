// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_CODE_H_
#define ARGON_OBJECT_CODE_H_

#include "object.h"

namespace argon::object {
    class Code : public Object {
        size_t hash_ = 0;
    public:
        size_t instr_sz;
        unsigned char *instr;

        bool EqualTo(const Object *other) override;

        size_t Hash() override;

        explicit Code(size_t instr_sz);

        ~Code() override;
    };

    inline const TypeInfo type_code_ = {
            .name=(const unsigned char *) "code",
            .size=sizeof(Code)
    };

} // namespace argon::object

#endif // !ARGON_OBJECT_CODE_H_
