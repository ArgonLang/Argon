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
    public:
        explicit String(const std::string &str);

        ~String() override;
    };
}


#endif // !ARGON_OBJECT_STRING_H_
