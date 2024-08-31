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

using namespace argon::vm::datatype;

ARGON_FUNCTION(builtins_bind, bind,
               "Return a partial-applied function(currying).\n"
               "\n"
               "Calling bind(func, args...) is equivalent to the following expression:\n"
               "\n\tfunc(args...)\n\n"
               "IF AND ONLY IF the number of arguments is less than the arity of the function, "
               "otherwise the expression invokes the function call.\n"
               "This does not happen with the use of bind which allows to bind a number of arguments "
               "equal to the arity of the function.\n"
               "\n"
               "- Parameters:\n"
               "    - func: callable object(function).\n"
               "    - ...obj: list of arguments to bind.\n"
               "- Returns: partial-applied function.\n",
               "F: func", true, false) {
    auto *func = (const Function *) args[0];

    if (argc - 1 > 0) {
        auto positional_args = argc - 1;

        if (func->currying != nullptr)
            positional_args += func->currying->length;

        if (positional_args > func->arity) {
            ErrorFormat(kTypeError[0], kTypeError[3], ARGON_RAW_STRING(func->qname), func->arity, positional_args);

            return nullptr;
        }

        return (ArObject *) FunctionNew(func, args + 1, argc - 1);
    }

    return IncRef(args[0]);
}

/*
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
*/

ARGON_FUNCTION(builtins_eval, eval,
               "Evaluate and execute string as Argon code.\n"
               "\n"
               "- Parameters:\n"
               "  - name: Input name.\n"
               "  - module: Module context in which to evaluate the argon code.\n"
               "  - src: Argon code.\n"
               "- KWParameters:\n"
               "  - optim: Set optimization level (0-3).\n"
               "- Returns: A result object that contains the result of the evaluation.\n",
               "s: name, m: module, sx: src", false, true) {
    ArBuffer buffer{};
    auto *f = argon::vm::GetFiber();
    int optim_lvl = 0;

    if (f != nullptr)
        optim_lvl = f->context->global_config->optim_lvl;

    optim_lvl = (int) DictLookupInt((Dict *) kwargs, "optim", optim_lvl);
    if (optim_lvl < 0 || optim_lvl > 3) {
        ErrorFormat(kValueError[0], "invalid optimization level. Expected a value between 0 and 3, got: %d", optim_lvl);
        return nullptr;
    }

    if (!BufferGet(args[2], &buffer, BufferFlags::READ))
        return nullptr;

    argon::lang::CompilerWrapper c_wrapper(optim_lvl);

    auto *code = c_wrapper.Compile((const char *) ARGON_RAW_STRING((String *) args[0]),
                                   (const char *) buffer.buffer,
                                   buffer.length);

    BufferRelease(&buffer);

    Result *result;

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

ARGON_FUNCTION(builtins_getattr, getattr,
               "Access object attributes dynamically by providing the object and the attribute name as arguments.\n"
               "\n"
               "- Parameters:\n"
               "  - obj: The object from which to retrieve the attribute.\n"
               "  - name: A string representing the name of the attribute you want to access.\n"
               "- KWParameters:\n"
               "  - default: A default value to return if the attribute does not exist.\n"
               "- Returns: If the attribute exists within the object, its value is returned, "
               "otherwise the default value if defined is returned.\n",
               ": obj, s: name", false, true) {
    bool _static = false;

    if (AR_GET_TYPE(*args) == type_type_)
        _static = true;

    auto *res = AttributeLoad(*args, args[1], _static);
    if (res == nullptr) {
        ArObject *def = nullptr;

        if (kwargs != nullptr && !DictLookup((Dict *) kwargs, "default", &def))
            return nullptr;

        if (def != nullptr)
            argon::vm::DiscardLastPanic();

        return def;
    }

    return res;
}

ARGON_FUNCTION(builtins_id, id,
               "Return the identity of an object.\n"
               "\n"
               "Returns a unique integer identifier for an object. This identifier remains constant throughout the object's lifetime. "
               "The same identifier may be reused for different objects that exist at separate times.\n"
               "\n"
               "- Parameter obj: Object to check.\n"
               "- Returns: Object memory address (UInt).\n",
               ": obj", false, false) {
    return (ArObject *) UIntNew(*((uintptr_t *) args));
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
               "Check if type implements all the indicated traits.\n"
               "\n"
               "- Parameters:\n"
               "  - obj: Type to check.\n"
               "  - ...traits: Traits list.\n"
               "- Returns: True if type implements ALL indicated traits, false otherwise.",
               ": obj, : traits", true, false) {
    if (!AR_TYPEOF(*args, type_type_)) {
        ErrorFormat(kTypeError[0], kTypeError[1], AR_TYPE_QNAME(*args));
        return nullptr;
    }

    for (ArSize i = 1; i < argc; i++) {
        if (!AR_TYPEOF(args[i], type_type_)) {
            ErrorFormat(kTypeError[0], kTypeError[1], AR_TYPE_QNAME(args[i]));
            return nullptr;
        }

        if (!TraitIsImplemented((const TypeInfo *) args[0], (TypeInfo *) args[i]))
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

/*
ARGON_FUNCTION(builtins_repr, repr,
               "Return a string containing a printable representation of an object.\n"
               "\n"
               "- Parameter obj: Object to get a printable representation from.\n"
               "- Returns: String version of object.\n",
               ": obj", false, false) {
    return Repr(*args);
}
*/

ARGON_FUNCTION(builtins_panicking, panicking,
               "Check if the current execution is in a panicking state.\n"
               "\n"
               "This function returns a boolean value indicating whether the Argon VM is currently in a panicking state. "
               "A panicking state typically occurs when an unhandled exception has been raised.\n"
               "\n"
               "Note: This function is intended to be used within a 'defer' call. In other contexts, "
               "it may not provide meaningful information and will likely always return False.\n"
               "\n"
               "- Returns: True if the VM is panicking, false otherwise.\n",
               nullptr, false, false) {
    return BoolToArBool(argon::vm::IsPanicking());
}

ARGON_FUNCTION(builtins_recover, recover,
               "Recover from a panic and retrieve the panic value.\n"
               "\n"
               "This function must be called inside a defer block. It stops the panic\n"
               "propagation and returns the panic value (usually an error object).\n"
               "\n"
               "If there is no active panic, recover() returns nil.\n"
               "\n"
               "Usage:\n"
               "  Inside a defer block, call recover() to handle panics:\n"
               "  - If a panic occurred, recover() returns the panic value and stops the panic.\n"
               "  - If no panic occurred, recover() returns nil.\n"
               "\n"
               "- Returns: The panic value if a panic is active, otherwise nil.\n",
               nullptr, false, false) {
    auto *err = argon::vm::GetLastError();
    if (err != nullptr)
        return err;

    return ARGON_NIL_VALUE;
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
    auto *path = (String *) *args;

    auto index = StringRFind(path, ".");
    if (index > 0) {
        path = StringSubs(path, 0, index);
        if (path == nullptr)
            return nullptr;
    }

    auto *mod = LoadModule(fiber->context->imp, path, nullptr);

    if (index >= 0)
        Release(path);

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

ARGON_FUNCTION(builtins_retval, retval,
               "Get or set the return value of the function that invoked the current defer block.\n"
               "\n"
               "This function can only be called inside a defer block. It affects the return value\n"
               "of the function that is executing the defer, not the defer function itself.\n"
               "\n"
               "If called without arguments, returns the current return value of the calling function.\n"
               "If called with an argument, sets the return value of the calling function to that argument.\n"
               "\n"
               "- Parameter value: Optional. The new return value to set for the calling function.\n"
               "- Returns: The current return value of the calling function.\n",
               nullptr, true, false) {
    auto *fiber = argon::vm::GetFiber();
    auto *frame = fiber->frame;

    if (!VariadicCheckPositional((const char *) ARGON_RAW_STRING(((Function *) _func)->name), argc, 0, 1))
        return nullptr;

    auto *back = frame->back;
    if (back == nullptr)
        return ARGON_NIL_VALUE;

    if (argc == 0)
        return NilOrValue(back->return_value);

    auto *old = NilOrValue(back->return_value);

    back->return_value = IncRef(args[0]);

    return old;
}

ARGON_FUNCTION(builtins_show, show,
               "Returns a list of names in the local scope or the attributes of the instance.\n"
               "\n"
               "Without arguments, returns a list with names in the current scope, with argument, returns a list "
               "with the instance attributes of the argument.\n"
               "\n"
               "- Parameter ...obj: object whose instance attributes you want to know.\n"
               "- Returns: List with attributes if any, otherwise an empty list.\n",
               "", true, false) {
    const auto *ancestor = (TypeInfo *) *args;
    Set *target = nullptr;
    Set *aux = nullptr;

    List *ret;

    if (!VariadicCheckPositional(builtins_show.name, (unsigned int) argc, 0, 1))
        return nullptr;

    if (argc == 0)
        return (ArObject *) NamespaceKeysToList(argon::vm::GetFiber()->frame->globals, {});

    if (AR_GET_TYPE(ancestor) != type_type_)
        ancestor = AR_GET_TYPE(ancestor);

    if (ancestor->tp_map != nullptr) {
        target = NamespaceKeysToSet((Namespace *) ancestor->tp_map, AttributeFlag::CONST | AttributeFlag::PUBLIC);
        if (target == nullptr)
            return nullptr;
    }

    if (AR_HAVE_OBJECT_BEHAVIOUR(*args) && AR_SLOT_OBJECT(*args)->namespace_offset >= 0) {
        aux = NamespaceKeysToSet(*((Namespace **) AR_GET_NSOFFSET(*args)), AttributeFlag::PUBLIC);
        if (aux == nullptr) {
            Release(target);

            return nullptr;
        }
    }

    if (target == nullptr && aux == nullptr)
        return (ArObject *) ListNew();

    if (target == nullptr) {
        ret = ListNew((ArObject *) aux);

        Release(aux);

        return (ArObject *) ret;
    }

    if (aux != nullptr) {
        if (!SetMerge(target, aux)) {
            Release(target);
            Release(aux);
        }

        Release(aux);
    }

    ret = ListNew((ArObject *) target);

    Release(target);

    return (ArObject *) ret;
}

/*
ARGON_FUNCTION(builtins_str, str,
               "Return a string version of an object.\n"
               "\n"
               "- Parameter obj: Object to represent as a string.\n"
               "- Returns: String version of object.\n",
               ": obj", false, false) {
    return Str(*args);
}
*/

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

        if (TypeOF(base, tp))
            return BoolToArBool(true);
    }

    return BoolToArBool(false);
}

const ModuleEntry builtins_entries[] = {
        MODULE_EXPORT_TYPE(type_atom_),
        MODULE_EXPORT_TYPE(type_boolean_),
        MODULE_EXPORT_TYPE(type_bounds_),
        MODULE_EXPORT_TYPE(type_bytes_),
        MODULE_EXPORT_TYPE(type_chan_),
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

        MODULE_EXPORT_FUNCTION(builtins_bind),
        //MODULE_EXPORT_FUNCTION(builtins_exit),
        MODULE_EXPORT_FUNCTION(builtins_eval),
        MODULE_EXPORT_FUNCTION(builtins_getattr),
        MODULE_EXPORT_FUNCTION(builtins_id),
        MODULE_EXPORT_FUNCTION(builtins_iscallable),
        MODULE_EXPORT_FUNCTION(builtins_implements),
        MODULE_EXPORT_FUNCTION(builtins_len),
        MODULE_EXPORT_FUNCTION(builtins_panicking),
        MODULE_EXPORT_FUNCTION(builtins_recover),
        MODULE_EXPORT_FUNCTION(builtins_require),
        MODULE_EXPORT_FUNCTION(builtins_retval),
        //MODULE_EXPORT_FUNCTION(builtins_repr),
        MODULE_EXPORT_FUNCTION(builtins_show),
        //MODULE_EXPORT_FUNCTION(builtins_str),
        MODULE_EXPORT_FUNCTION(builtins_type),
        MODULE_EXPORT_FUNCTION(builtins_typeof),
        ARGON_MODULE_SENTINEL
};

const char *builtins_native = R"(

pub func exit() {
    /*
        Exit by initiating a panicking state with RuntimeExit error.

        This is a convenient function to terminate your interactive session.

        - Returns: This function does not return to the caller.
    */

    panic Error(@RuntimeExit, "")
}

pub func hash(obj) {
    /*
        Return hash value of an object if it has one.

        - Parameter obj: Object which we need to convert into hash.
        - Returns: Returns the hashed value if possible.
    */

    meth := obj.__hash
    if !meth.__method {
        panic(Error(@TypeError, "expected '%s' as method, got function" % meth.__qname))
    }

    return meth(obj)
}

pub func str(obj) {
    /*
        Return a string version of an object.

        - Parameter obj: Object to represent as a string.
        - Returns: String version of object.
    */

    meth := obj.__str
    if !meth.__method {
        panic(Error(@TypeError, "expected '%s' as method, got function" % meth.__qname))
    }

    return meth(obj)
}

pub func repr(obj) {
    /*
        Return a string containing a printable representation of an object.

        - Parameter obj: Object to get a printable representation from.
        - Returns: String version of object.
    */

    meth := obj.__repr
    if !meth.__method {
        panic(Error(@TypeError, "expected '%s' as method, got function" % meth.__qname))
    }

    return meth(obj)
}

)";

bool BuiltinsInit(Module *self) {
    argon::lang::CompilerWrapper cw;

    auto *code = cw.Compile("builtins", builtins_native);
    if (code == nullptr)
        return false;

    auto *result = argon::vm::Eval(nullptr, code, self->ns);
    if (result == nullptr)
        return false;

    Release(result);

    return true;
}

constexpr ModuleInit ModuleBuiltins = {
        "argon:builtins",
        "Built-in functions and other things.",
        nullptr,
        builtins_entries,
        BuiltinsInit,
        nullptr
};
const ModuleInit *argon::vm::mod::module_builtins_ = &ModuleBuiltins;
