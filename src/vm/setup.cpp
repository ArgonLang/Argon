// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/datatype/arobject.h>
#include <vm/datatype/arstring.h>
#include <vm/datatype/atom.h>
#include <vm/datatype/boolean.h>
#include <vm/datatype/bounds.h>
#include <vm/datatype/bytes.h>
#include <vm/datatype/code.h>
#include <vm/datatype/decimal.h>
#include <vm/datatype/dict.h>
#include <vm/datatype/error.h>
#include <vm/datatype/function.h>
#include <vm/datatype/future.h>
#include <vm/datatype/integer.h>
#include <vm/datatype/list.h>
#include <vm/datatype/module.h>
#include <vm/datatype/namespace.h>
#include <vm/datatype/nil.h>
#include <vm/datatype/result.h>
#include <vm/datatype/set.h>
#include <vm/datatype/tuple.h>

#include <vm/importer/import.h>

#include "setup.h"

using namespace argon::vm;
using namespace argon::vm::datatype;

bool argon::vm::Setup() {
#define INIT(type)                              \
    if(!TypeInit((TypeInfo *)(type), nullptr))  \
        return false

    if (!ErrorInit())
        return false;

    INIT(type_atom_);
    INIT(type_boolean_);
    INIT(type_bounds_);
    INIT(type_bytes_);
    INIT(type_code_);
    INIT(type_decimal_);
    INIT(type_dict_);
    INIT(type_error_);
    INIT(type_function_);
    INIT(type_future_);
    INIT(type_int_);
    INIT(type_list_);
    INIT(type_module_);
    INIT(type_namespace_);
    INIT(type_nil_);
    INIT(type_result_);
    INIT(type_set_);
    INIT(type_string_);
    INIT(type_tuple_);
    INIT(type_uint_);

    INIT(importer::type_import_spec_);
    INIT(importer::type_import_);

    return true;
}