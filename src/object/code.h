// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_CODE_H_
#define ARGON_OBJECT_CODE_H_

#include "object.h"
#include "list.h"

namespace argon::object {
    class Code : public Object {
        size_t hash_ = 0;
    public:
        argon::object::List *statics = nullptr;
        argon::object::List *names = nullptr;
        argon::object::List *locals = nullptr;

        const unsigned char *instr = nullptr;

        const unsigned int instr_sz;
        const unsigned int stack_sz;

        bool EqualTo(const Object *other) override;

        size_t Hash() override;

        explicit Code(const unsigned char *instr, unsigned int instr_sz, unsigned int stack_sz,
                      argon::object::List *statics, argon::object::List *names, argon::object::List *locals);

        ~Code() override;
    };

    inline const TypeInfo type_code_ = {
            .name=(const unsigned char *) "code",
            .size=sizeof(Code)
    };

} // namespace argon::object

#endif // !ARGON_OBJECT_CODE_H_
