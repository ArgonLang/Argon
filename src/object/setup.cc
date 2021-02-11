// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/list.h>
#include <object/datatype/option.h>
#include <object/datatype/map.h>
#include <object/datatype/set.h>
#include <object/datatype/tuple.h>
#include <object/datatype/string.h>

#include "arobject.h"
#include "setup.h"

using namespace argon::object;

bool argon::object::TypesInit() {
#define INIT_TYPE(type)                 \
    if(!TypeInit((TypeInfo*)&(type)))   \
        return false

    INIT_TYPE(type_list_);
    INIT_TYPE(type_option_);
    INIT_TYPE(type_tuple_);
    INIT_TYPE(type_map_);
    INIT_TYPE(type_set_);
    INIT_TYPE(type_string_);

    return true;
#undef INIT_TYPE
}