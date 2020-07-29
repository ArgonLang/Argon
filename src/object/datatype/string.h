// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_STRING_H_
#define ARGON_OBJECT_STRING_H_

#include <string>
#include <cstring>

#include <object/objmgmt.h>

namespace argon::object {
    struct String : public ArObject {
        unsigned char *buffer;
        size_t len;
        size_t hash;
    };

    extern const TypeInfo type_string_;

    String *StringNew(const char *string, size_t len);

    inline String *StringNew(const char *string) { return StringNew(string, strlen(string)); }

    inline String *StringNew(const std::string &string) { return StringNew(string.c_str(), string.length()); }

    String *StringIntern(const std::string &string);

    bool StringEq(String *string, const unsigned char *c_str, size_t len);
}


#endif // !ARGON_OBJECT_STRING_H_
