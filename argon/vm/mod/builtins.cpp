// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/lang/compiler_wrapper.h>

#include <argon/vm/runtime.h>

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/atom.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/bounds.h>
#include <argon/vm/datatype/bytes.h>
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

using namespace argon::vm::datatype;

ARGON_FUNCTION(builtins_exit, exit,
               "Exit by initiating a panicking state with RuntimeExit error.\n"
               "\n"
               "This is a convenient function to terminate your interactive session.\n"
               "\n"
               "- Returns: This function does not return to the caller.",
               nullptr, false, false) {
    ErrorFormat(kRuntimeExitError[0], "");
    return nullptr;
}

ARGON_FUNCTION(builtins_eval, eval,
               "Evaluate and execute string as Argon code.\n"
               "\n"
               "- Parameters:\n"
               "  - name: Input name.\n"
               "  - module: Module context in which to evaluate the argon code.\n"
               "  - src: Argon code.\n"
               "- Returns: A result object that contains the result of the evaluation.\n",
               "s: name, m: module, sx: src", false, false) {
    ArBuffer buffer{};
    Result *result;

    if (!BufferGet(args[2], &buffer, BufferFlags::READ))
        return nullptr;

    argon::lang::CompilerWrapper c_wrapper;

    auto *code = c_wrapper.Compile((const char *) ARGON_RAW_STRING((String *) args[0]),
                                   (const char *) buffer.buffer,
                                   buffer.length);

    BufferRelease(&buffer);

    if (code == nullptr) {
        auto *err = argon::vm::GetLastError();

        result = ResultNew(err, false);

        Release(err);
    } else {
        result = Eval(argon::vm::GetFiber()->context, code, ((Module *) args[1])->ns);
        Release(code);
    }

    return (ArObject *) result;
}

ARGON_FUNCTION(builtins_iscallable, iscallable,
               "Return true if argument appears callable, false otherwise.\n"
               "\n"
               "- Parameter obj: Object to check.\n"
               "- Returns: True if object is callable, false otherwise.\n",
               ": obj", false, false) {
    ArObject *ret;
    String *key;

    if (AR_TYPEOF(*args, type_function_))
        return BoolToArBool(true);

    if (AR_TYPEOF(*args, type_type_)) {
        if ((key = StringNew(((TypeInfo *) *args)->name)) == nullptr)
            return nullptr;

        ret = AttributeLoad(*args, (ArObject *) key, true);

        Release(ret);
        Release(key);

        if (ret != nullptr)
            return BoolToArBool(true);

        argon::vm::DiscardLastPanic();
    }

    return BoolToArBool(false);
}

ARGON_FUNCTION(builtins_implements, implements,
               "Check if object implements all the indicated traits.\n"
               "\n"
               "- Parameters:\n"
               "  - obj: Object to check.\n"
               "  - ...traits: Traits list.\n"
               "- Returns: True if the object implements ALL indicated traits, false otherwise.",
               ": obj, : traits", true, false) {
    for (ArSize i = 1; i < argc; i++) {
        if (!TraitIsImplemented(args[0], (TypeInfo *) args[i]))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ARGON_FUNCTION(builtins_len, len,
               "Returns the length of an object.\n"
               "\n"
               "- Parameter obj: Object to check.\n"
               "- Returns: The length of the object.\n",
               ": obj", false, false) {
    if (AR_SLOT_SUBSCRIPTABLE(*args) != nullptr && AR_SLOT_SUBSCRIPTABLE(*args)->length != nullptr)
        return (ArObject *) IntNew(AR_SLOT_SUBSCRIPTABLE(*args)->length(*args));

    ErrorFormat(kTypeError[0], "'%s' have no length", AR_TYPE_QNAME(*args));
    return nullptr;
}

ARGON_FUNCTION(builtins_repr, repr,
               "Return a string containing a printable representation of an object.\n"
               "\n"
               "- Parameter obj: Object to get a printable representation from.\n"
               "- Returns: String version of object.\n",
               ": obj", false, false) {
    return Repr(*args);
}

ARGON_FUNCTION(builtins_require, require,
               "Allows you to dynamically import a module.\n"
               "\n"
               "- Parameter name: Module name.\n"
               "- Returns: A result object that can contain a loaded module.\n",
               "s: name", false, false) {
    ArObject *error;
    Result *result;

    auto *fiber = argon::vm::GetFiber();

    auto *mod = LoadModule(fiber->context->imp, ((String *) *args), nullptr);
    if (mod != nullptr) {
        if ((result = ResultNew((ArObject *) mod, true)) == nullptr)
            Release(mod);

        return (ArObject *) result;
    }

    error = argon::vm::GetLastError();

    if ((result = ResultNew(error, false)) == nullptr)
        Release(error);

    return (ArObject *) result;
}

ARGON_FUNCTION(builtins_str, str,
               "Return a string version of an object.\n"
               "\n"
               "- Parameter obj: Object to represent as a string.\n"
               "- Returns: String version of object.\n",
               ": obj", false, false) {
    return Str(*args);
}

ARGON_FUNCTION(builtins_type, type,
               "Returns type of the object passed as parameter.\n"
               "\n"
               "- Parameter obj: Object to get the type from.\n"
               "- Returns: Object type.\n",
               ": obj", false, false) {
    return IncRef((ArObject *) AR_GET_TYPE(*args));
}

ARGON_FUNCTION(builtins_typeof, typeof,
               "Verify that the type of the object is one of the ones passed.\n"
               "\n"
               "- Parameters:"
               "  - obj: Object to check.\n"
               "  - ...types: Types to compare.\n"
               "- Returns: True if a type matches the object's type, false otherwise.\n",
               ": obj", true, false) {
    const auto *base = *args;

    if (!VariadicCheckPositional(builtins_typeof.name, (unsigned int) argc - 1, 1, 0))
        return nullptr;

    for (auto i = 1; i < argc; i++) {
        auto *tp = (const TypeInfo *) args[i];

        if (!AR_TYPEOF(tp, type_type_))
            tp = AR_GET_TYPE(tp);

        if (AR_TYPEOF(base, tp))
            return BoolToArBool(true);
    }

    return BoolToArBool(false);
}

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
        MODULE_EXPORT_TYPE(type_option_),
        MODULE_EXPORT_TYPE(type_result_),
        MODULE_EXPORT_TYPE(type_set_),
        MODULE_EXPORT_TYPE(type_string_),
        MODULE_EXPORT_TYPE(type_tuple_),
        MODULE_EXPORT_TYPE(type_uint_),

        MODULE_EXPORT_FUNCTION(builtins_exit),
        MODULE_EXPORT_FUNCTION(builtins_eval),
        MODULE_EXPORT_FUNCTION(builtins_iscallable),
        MODULE_EXPORT_FUNCTION(builtins_implements),
        MODULE_EXPORT_FUNCTION(builtins_len),
        MODULE_EXPORT_FUNCTION(builtins_require),
        MODULE_EXPORT_FUNCTION(builtins_repr),
        MODULE_EXPORT_FUNCTION(builtins_str),
        MODULE_EXPORT_FUNCTION(builtins_type),
        MODULE_EXPORT_FUNCTION(builtins_typeof),
        ARGON_MODULE_SENTINEL
};

constexpr ModuleInit ModuleBuiltins = {
        "argon:builtins",
        "Built-in functions and other things.",
        builtins_entries,
        nullptr,
        nullptr
};
const ModuleInit *argon::vm::mod::module_builtins_ = &ModuleBuiltins;