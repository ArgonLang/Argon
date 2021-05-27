// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_OPTION_H_
#define ARGON_OBJECT_OPTION_H_

#include <object/arobject.h>

namespace argon::object {
    struct Option : ArObject {
        ArObject *some;
    };

    extern const TypeInfo *type_option_;

    Option *OptionNew(ArObject *obj);

    inline Option *OptionNew() { return OptionNew(nullptr); }
}

#endif // !ARGON_OBJECT_OPTION_H_
