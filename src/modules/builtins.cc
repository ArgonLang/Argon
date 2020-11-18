// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/bool.h>
#include <object/datatype/bounds.h>
#include <object/datatype/code.h>
#include <object/datatype/decimal.h>
#include <object/datatype/error.h>
#include <object/datatype/function.h>
#include <object/datatype/instance.h>
#include <object/datatype/integer.h>
#include <object/datatype/list.h>
#include <object/datatype/map.h>
#include <object/datatype/module.h>
#include <object/datatype/namespace.h>
#include <object/datatype/nil.h>
#include <object/datatype/string.h>
#include <object/datatype/struct.h>
#include <object/datatype/trait.h>
#include <object/datatype/tuple.h>

#include "builtins.h"

using namespace argon::object;

ARGON_FUNC_NATIVE(builtins_type, type, "Returns type of the argument passed as parameter.", 1, false) {
    IncRef((ArObject *) argv[0]->type);
    return (ArObject *) argv[0]->type;
}

const PropertyBulk builtins_bulk[] = {
        MODULE_BULK_EXPORT_TYPE("bool", type_bool_),
        MODULE_BULK_EXPORT_TYPE("bounds", type_bounds_),
        MODULE_BULK_EXPORT_TYPE("code", type_code_),
        MODULE_BULK_EXPORT_TYPE("decimal", type_decimal_),
        MODULE_BULK_EXPORT_TYPE("func", type_function_),
        MODULE_BULK_EXPORT_TYPE("instance", type_instance_),
        MODULE_BULK_EXPORT_TYPE("integer", type_integer_),
        MODULE_BULK_EXPORT_TYPE("list", type_list_),
        MODULE_BULK_EXPORT_TYPE("map", type_map_),
        MODULE_BULK_EXPORT_TYPE("module", type_module_),
        MODULE_BULK_EXPORT_TYPE("namespace", type_namespace_),
        MODULE_BULK_EXPORT_TYPE("nil", type_nil_),
        MODULE_BULK_EXPORT_TYPE("str", type_string_),
        MODULE_BULK_EXPORT_TYPE("struct", type_struct_),
        MODULE_BULK_EXPORT_TYPE("trait", type_trait_),
        MODULE_BULK_EXPORT_TYPE("tuple", type_tuple_),
        {"type", {.func=&builtins_type}, true, PropertyInfo(PropertyType::CONST | PropertyType::PUBLIC)},
        {nullptr, nullptr, false, PropertyInfo()} // Sentinel
};

const ModuleInit module_builtins = {
        "builtins",
        "Built-in functions and other things",
        builtins_bulk
};

Module *argon::modules::BuiltinsNew() {
    return ModuleNew(&module_builtins);
}