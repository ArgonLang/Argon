// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "arobject.h"
#include "arstring.h"
#include "atom.h"
#include "boolean.h"
#include "bytes.h"
#include "decimal.h"
#include "dict.h"
#include "error.h"
#include "function.h"
#include "integer.h"
#include "list.h"
#include "namespace.h"
#include "nil.h"
#include "set.h"
#include "tuple.h"

#include "setup.h"

using namespace argon::vm::datatype;

bool argon::vm::datatype::Setup() {
#define INIT(type)                  \
    if(!TypeInit((type), nullptr))  \
        return false

    if(!ErrorInit())
        return false;

    INIT(type_atom_);
    INIT(type_boolean_);
    INIT(type_bytes_);
    INIT(type_decimal_);
    INIT(type_dict_);
    INIT(type_function_);
    INIT(type_int_);
    INIT(type_list_);
    INIT(type_namespace_);
    INIT(type_nil_);
    INIT(type_set_);
    INIT(type_string_);
    INIT(type_tuple_);

    return true;
}