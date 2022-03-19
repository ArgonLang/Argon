// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MODULE_REGEX_H_
#define ARGON_MODULE_REGEX_H_

#include <regex>

#include <object/arobject.h>
#include <object/datatype/string.h>

namespace argon::module {

    struct Pattern : argon::object::ArObject {
        argon::object::String *init_str;
        std::regex *pattern;
    };

    extern const argon::object::TypeInfo *type_re_pattern_;

    struct Match : argon::object::ArObject{
        argon::object::ArSize start;
        argon::object::ArSize end;

        argon::object::ArObject *match;
    };

    extern const argon::object::TypeInfo *type_re_match_;
} // namespace argon::module

#endif // !ARGON_MODULE_REGEX_H_
