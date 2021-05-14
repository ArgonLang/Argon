// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/list.h>
#include <object/datatype/option.h>
#include <object/datatype/map.h>
#include <object/datatype/set.h>
#include <object/datatype/tuple.h>
#include <object/datatype/string.h>
#include <object/datatype/error.h>
#include <object/datatype/bytes.h>
#include <object/datatype/bool.h>
#include <object/datatype/decimal.h>

#include "arobject.h"
#include "setup.h"

using namespace argon::object;

bool argon::object::TypesInit() {
#define INIT_TYPE(type)                         \
    if(!TypeInit((TypeInfo*)&(type), nullptr))  \
        return false

    if (!ErrorInit())
        return false;

    INIT_TYPE(type_bool_);
    INIT_TYPE(type_decimal_);
    INIT_TYPE(type_integer_);
    INIT_TYPE(type_bytes_);
    INIT_TYPE(type_map_);
    INIT_TYPE(type_option_);
    INIT_TYPE(type_set_);
    INIT_TYPE(type_string_);
    INIT_TYPE(type_tuple_);
    INIT_TYPE(type_list_);

    return true;
#undef INIT_TYPE
}