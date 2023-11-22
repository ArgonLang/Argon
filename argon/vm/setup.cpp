// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/atom.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/bounds.h>
#include <argon/vm/datatype/bytes.h>
#include <argon/vm/datatype/chan.h>
#include <argon/vm/datatype/code.h>
#include <argon/vm/datatype/decimal.h>
#include <argon/vm/datatype/dict.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/function.h>
#include <argon/vm/datatype/future.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/list.h>
#include <argon/vm/datatype/module.h>
#include <argon/vm/datatype/namespace.h>
#include <argon/vm/datatype/nil.h>
#include <argon/vm/datatype/option.h>
#include <argon/vm/datatype/result.h>
#include <argon/vm/datatype/set.h>
#include <argon/vm/datatype/tuple.h>

#include <argon/vm/io/io.h>

#include <argon/vm/importer/import.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <argon/vm/support/nt/handle.h>

#endif

#include <argon/vm/setup.h>

using namespace argon::vm;
using namespace argon::vm::datatype;

bool argon::vm::Setup() {
#define INIT(type)                              \
    if(!TypeInit((TypeInfo *)(type), nullptr))  \
        return false

    if (!ErrorInit())
        return false;

    if (!io::IOInit())
        return false;

    INIT(type_type_);

    INIT(type_atom_);
    INIT(type_boolean_);
    INIT(type_bounds_);
    INIT(type_bytes_);
    INIT(type_chan_);
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
    INIT(type_option_);
    INIT(type_result_);
    INIT(type_set_);
    INIT(type_string_);
    INIT(type_tuple_);
    INIT(type_uint_);

    INIT(importer::type_import_spec_);
    INIT(importer::type_import_);

#ifdef _ARGON_PLATFORM_WINDOWS
    INIT(support::nt::type_oshandle_);
#endif

    return true;
}