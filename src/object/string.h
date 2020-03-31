// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_STRING_H_
#define ARGON_OBJECT_STRING_H_

#include <string>

#include "object.h"

namespace argon::object {
    class String : public Object {
        unsigned char *buffer_;
        size_t len_;
        size_t hash_;
    public:
        explicit String(const std::string &str);

        ~String() override;

        bool EqualTo(const Object *other) override;

        size_t Hash() override;
    };

    inline const TypeInfo type_string_ = {
            .name=(const unsigned char *) "string",
            .size=sizeof(String)
    };
}


#endif // !ARGON_OBJECT_STRING_H_
