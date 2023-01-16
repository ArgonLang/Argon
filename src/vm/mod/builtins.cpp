// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

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

#include "modules.h"

using namespace argon::vm::datatype;

const ModuleEntry builtins_entries[] = {
        MODULE_EXPORT_TYPE(type_atom_),
        MODULE_EXPORT_TYPE(type_boolean_),
        MODULE_EXPORT_TYPE(type_bounds_),
        MODULE_EXPORT_TYPE(type_bytes_),
        MODULE_EXPORT_TYPE(type_code_),
        MODULE_EXPORT_TYPE(type_decimal_),
        MODULE_EXPORT_TYPE(type_dict_),
        MODULE_EXPORT_TYPE(type_error_),
        MODULE_EXPORT_TYPE(type_function_),
        MODULE_EXPORT_TYPE(type_future_),
        MODULE_EXPORT_TYPE(type_int_),
        MODULE_EXPORT_TYPE(type_list_),
        MODULE_EXPORT_TYPE(type_module_),
        MODULE_EXPORT_TYPE(type_namespace_),
        MODULE_EXPORT_TYPE(type_nil_),
        MODULE_EXPORT_TYPE(type_result_),
        MODULE_EXPORT_TYPE(type_set_),
        MODULE_EXPORT_TYPE(type_tuple_),
        MODULE_EXPORT_TYPE(type_uint_),
        ARGON_MODULE_SENTINEL
};

constexpr ModuleInit ModuleBuiltins = {
        "builtins",
        "Built-in functions and other things.",
        builtins_entries,
        nullptr,
        nullptr
};
const ModuleInit *argon::vm::mod::module_builtins_ = &ModuleBuiltins;