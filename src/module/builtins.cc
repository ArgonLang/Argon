// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/context.h>
#include <vm/runtime.h>

#include <object/datatype/bool.h>
#include <object/datatype/bounds.h>
#include <object/datatype/bytes.h>
#include <object/datatype/code.h>
#include <object/datatype/decimal.h>
#include <object/datatype/error.h>
#include <object/datatype/function.h>
#include "object/datatype/io/io.h"
#include <object/datatype/integer.h>
#include <object/datatype/list.h>
#include <object/datatype/map.h>
#include <object/datatype/module.h>
#include <object/datatype/namespace.h>
#include <object/datatype/nil.h>
#include <object/datatype/option.h>
#include <object/datatype/set.h>
#include <object/datatype/string.h>
#include <object/datatype/tuple.h>

#include "builtins.h"

using namespace argon::object;
using namespace argon::module;

ARGON_FUNCTION(callable,
               "Return true if argument appears callable, false otherwise."
               ""
               "- Parameter obj: object to check."
               "- Returns: true if object is callable, false otherwise.",
               1, false) {
    // This definition may be change in future
    if (argv[0]->type == type_function_)
        return True;
    return False;
}

ARGON_FUNCTION(hasnext,
               "Return true if the iterator has more elements."
               ""
               "- Parameter iterator: iterator object."
               "- Returns: true if the iterator has more elements, false otherwise.",
               1, false) {
    return BoolToArBool(AR_ITERATOR_SLOT(*argv)->has_next(*argv));
}

ARGON_FUNCTION(input,
               "Allowing user input."
               ""
               "- Parameter prompt: string representing a default message before the input."
               "- Returns: string containing user input.", 1, false) {
    auto in = (io::File *) argon::vm::ContextRuntimeGetProperty("stdin", io::type_file_);
    auto out = (io::File *) argon::vm::ContextRuntimeGetProperty("stdout", io::type_file_);
    unsigned char *line = nullptr;
    ArObject *str = nullptr;
    ArSSize len;

    if (in == nullptr || out == nullptr)
        goto error;

    if ((str = ToString(argv[0])) == nullptr)
        goto error;

    if (io::WriteObject(out, str) < 0)
        goto error;

    io::Flush(out);
    Release(str);
    Release(out);

    if ((len = io::ReadLine(in, &line, 0)) < 0)
        goto error;

    Release(in);
    return StringNewBufferOwnership(line, len);

    error:
    Release(in);
    Release(out);
    Release(str);
    return nullptr;
}

ARGON_FUNCTION(isinstance,
               "Check if object is an instance of indicated type."
               ""
               "- Parameter obj: object to check."
               "- Returns: true if the object is an instance of indicated type, false otherwise.", 2, false) {
    return BoolToArBool(argv[0]->type == argv[1]);
}

ARGON_FUNCTION(isimpl,
               "Check if object implements all the indicated traits."
               ""
               "- Parameters:"
               "    - obj: object to check."
               "    - ...traits: traits list."
               "- Returns: true if the object implements ALL indicated traits, false otherwise.", 2, true) {
    for (ArSize i = 1; i < count; i++) {
        if (!TraitIsImplemented(argv[0], (TypeInfo *) argv[i]))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ARGON_FUNCTION(iter,
               "Return an iterator object."
               ""
               "- Parameter obj: iterable object."
               "- Returns: new iterator."
               "- Panic TypeError: object is not iterable."
               ""
               "# SEE"
               "- riter: to obtain a reverse iterator.",
               1, false) {
    return IteratorGet(*argv);
}

ARGON_FUNCTION(len,
               "Returns the length of an object."
               ""
               "- Parameter obj: object to check."
               "- Returns: the length of the object."
               "- Panics:"
               "  - TypeError: object has no len."
               "  - OverflowError: object is too long.",
               1, false) {
    ArSize length;

    if (AsSequence(argv[0]))
        length = argv[0]->type->sequence_actions->length(argv[0]);
    else if (AsMap(argv[0]))
        length = argv[0]->type->map_actions->length(argv[0]);
    else
        return ErrorFormat(type_type_error_, "type '%s' has no len", argv[0]->type->name);

    return IntegerNew(length);
}

ARGON_FUNCTION(next,
               "Retrieve the next item from the iterator."
               ""
               "- Parameter iterator: iterator object."
               "- Returns: object."
               "- Panics:"
               "     - TypeError: invalid iterator."
               "     - ExhaustedIteratorError: reached the end of the collection.",
               1, false) {
    ArObject *ret = IteratorNext(*argv);

    if (ret == nullptr)
        return ErrorFormat(type_exhausted_iterator_, "reached the end of the collection");

    return ret;
}

ARGON_FUNCTION(panic,
               "Stops normal execution of current ArRoutine."
               ""
               "When a function F calls panic, it's execution stops immediately, "
               "after that, any deferred function run in usual way, and then F returns to its caller."
               "To the caller, the invocation of function F behaves like a call to panic,"
               "terminating caller function and executing any deferred functions."
               "This behaviour continues until all function in the current ArRoutine have stopped."
               "At that point, the program terminated with a non-zero exit code."
               "You can control this termination sequence (panicking) using the built-in function recover."
               ""
               "- Parameter obj: an object that describe this error."
               "- Returns: this function does not return to the caller.",
               1, false) {
    return argon::vm::Panic(argv[0]);
}

ARGON_FUNCTION(recover,
               "Allows a program to manage behavior of panicking ArRoutine."
               ""
               "Executing a call to recover inside a deferred function stops"
               "the panicking sequence by restoring normal execution flow."
               "After that the function retrieve and returns the error value passed"
               "to the call of function panic."
               ""
               "# WARNING"
               "Calling this function outside of deferred function has no effect."
               ""
               "- Returns: argument supplied to panic call, or nil if ArRoutine is not panicking.",
               0, false) {
    return ReturnNil(argon::vm::GetLastError());
}

ARGON_FUNCTION(returns,
               "Set and/or get the return value of the function that invoked a defer."
               ""
               "If returns is called with:"
               "    * 0 argument: no value is set as a return value."
               "    * 1 argument: argument is set as a return value."
               "    * n arguments: the return value is a tuple containing all the passed values."
               ""
               "In any case, the current return value is returned."
               ""
               "- Parameters:"
               "    - ...objs: return value."
               "- Returns: current return value.", 0, true) {
    ArObject *ret;
    ArObject *current;

    current = ReturnNil(argon::vm::RoutineReturnGet(argon::vm::GetRoutine()));

    if (count > 0) {
        if (count > 1) {
            if ((ret = TupleNew(argv, count)) == nullptr)
                return nullptr;

            argon::vm::RoutineReturnSet(argon::vm::GetRoutine(), ret);
            Release(ret);

            return current;
        }

        argon::vm::RoutineReturnSet(argon::vm::GetRoutine(), *argv);
    }

    return current;
}

ARGON_FUNCTION(riter,
               "Return an reverse iterator object."
               ""
               "- Parameter obj: iterable object."
               "- Returns: new reverse iterator."
               "- Panic TypeError: object is not iterable."
               ""
               "# SEE"
               "- iter: to obtain an iterator.",
               1, false) {
    return IteratorGetReversed(*argv);
}

ARGON_FUNCTION(type,
               "Returns type of the object passed as parameter."
               ""
               "- Parameter obj: object to get the type from."
               "- Returns: obj type.",
               1, false) {
    IncRef((ArObject *) argv[0]->type);
    return (ArObject *) argv[0]->type;
}

ARGON_FUNCTION(print,
               "Print objects to the stdout, separated by space."
               ""
               "- Parameters:"
               "     - ...obj: objects to print."
               "- Returns: nil",
               0, true) {
    auto out = (io::File *) argon::vm::ContextRuntimeGetProperty("stdout", io::type_file_);
    ArObject *str;
    ArSize i = 0;

    if (out == nullptr)
        return nullptr;

    while (i < count) {
        if ((str = ToString(argv[i++])) == nullptr)
            return nullptr;

        if (io::WriteObject(out, str) < 0) {
            Release(str);
            return nullptr;
        }

        Release(str);

        if (i < count) {
            if (io::Write(out, (unsigned char *) " ", 1) < 0)
                return nullptr;
        }
    }

    return NilVal;
}

ARGON_FUNCTION(println,
               "Same as print, but add new-line at the end."
               ""
               "- Parameters:"
               "     - ...obj: objects to print."
               "- Returns: nil"
               ""
               "# SEE"
               "- print.",
               0, true) {
    ArObject *success = ARGON_CALL_FUNC(print, origin, self, argv, count);

    if (success != nullptr) {
        auto out = (io::File *) argon::vm::ContextRuntimeGetProperty("stdout", io::type_file_);

        if (out == nullptr)
            return nullptr;

        if (io::Write(out, (unsigned char *) "\n", 1) < 0) {
            Release(success);
            return nullptr;
        }
    }

    return success;
}

const PropertyBulk builtins_bulk[] = {
        MODULE_EXPORT_TYPE_ALIAS("bool", type_bool_),
        MODULE_EXPORT_TYPE_ALIAS("bounds", type_bounds_),
        MODULE_EXPORT_TYPE_ALIAS("bytes", type_bytes_),
        MODULE_EXPORT_TYPE_ALIAS("code", type_code_),
        MODULE_EXPORT_TYPE_ALIAS("decimal", type_decimal_),
        MODULE_EXPORT_TYPE_ALIAS("function", type_function_),
        MODULE_EXPORT_TYPE_ALIAS("integer", type_integer_),
        MODULE_EXPORT_TYPE_ALIAS("list", type_list_),
        MODULE_EXPORT_TYPE_ALIAS("map", type_map_),
        MODULE_EXPORT_TYPE_ALIAS("module", type_module_),
        MODULE_EXPORT_TYPE_ALIAS("namespace", type_namespace_),
        MODULE_EXPORT_TYPE_ALIAS("niltype", type_nil_),
        MODULE_EXPORT_TYPE_ALIAS("option", type_option_),
        MODULE_EXPORT_TYPE_ALIAS("set", type_set_),
        MODULE_EXPORT_TYPE_ALIAS("str", type_string_),
        MODULE_EXPORT_TYPE_ALIAS("tuple", type_tuple_),

        // Functions
        MODULE_EXPORT_FUNCTION(callable_),
        MODULE_EXPORT_FUNCTION(input_),
        MODULE_EXPORT_FUNCTION(isinstance_),
        MODULE_EXPORT_FUNCTION(isimpl_),
        MODULE_EXPORT_FUNCTION(iter_),
        MODULE_EXPORT_FUNCTION(hasnext_),
        MODULE_EXPORT_FUNCTION(len_),
        MODULE_EXPORT_FUNCTION(next_),
        MODULE_EXPORT_FUNCTION(panic_),
        MODULE_EXPORT_FUNCTION(print_),
        MODULE_EXPORT_FUNCTION(println_),
        MODULE_EXPORT_FUNCTION(recover_),
        MODULE_EXPORT_FUNCTION(returns_),
        MODULE_EXPORT_FUNCTION(riter_),
        MODULE_EXPORT_FUNCTION(type_),

        MODULE_EXPORT_SENTINEL
};

const ModuleInit module_builtins = {
        "builtins",
        "Built-in functions and other things",
        builtins_bulk
};

Module *argon::module::BuiltinsNew() {
    return ModuleNew(&module_builtins);
}