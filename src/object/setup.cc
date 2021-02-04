// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arobject.h"
#include "setup.h"

using namespace argon::object;

bool argon::object::TypesInit() {
#define INIT_TYPE(type)                 \
    if(!TypeInit((TypeInfo*)&(type)))   \
        return false

    return true;
#undef INIT_TYPE
}