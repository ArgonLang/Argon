// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_STRING_H_
#define ARGON_OBJECT_STRING_H_

#include <string>

#include "object.h"

namespace argon::object {
    struct String : public ArObject {
        unsigned char *buffer;
        size_t len;
        size_t hash;
    };

    extern const TypeInfo type_string_;

    String *StringNew(const std::string &string);

    String *StringIntern(const std::string &string);

    bool StringEq(String *string, const unsigned char *c_str, size_t len);
}


#endif // !ARGON_OBJECT_STRING_H_
