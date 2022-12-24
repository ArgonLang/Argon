// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/context.h>
#include <vm/runtime.h>

#include <object/datatype/atom.h>
#include <object/datatype/bool.h>
#include <object/datatype/bounds.h>
#include <object/datatype/bytes.h>
#include <object/datatype/code.h>
#include <object/datatype/decimal.h>
#include <object/datatype/error.h>
#include <object/datatype/frame.h>
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

#include "modules.h"

using namespace argon::object;
using namespace argon::module;

ARGON_FUNCTION(bind,
               "Return a partial-applied function(currying).\n"
               "\n"
               "Calling bind(func, args...) is equivalent to the following expression:\n"
               "func(args...) "
               "IF AND ONLY IF the number of arguments is less than the arity of the function,"
               "otherwise the expression invokes the function call. \n"
               "This does not happen with the use of bind which allows to bind a number of arguments"
               "equal to the arity of the function.\n"
               "\n"
               "- Parameters:\n"
               "    - func: callable object(function).\n"
               "    - ...obj: list of arguments to bind.\n"
               "- Returns: partial-applied function.",
               1, true) {
    auto *base = (Function *) argv[0];
    Function *fnew;
    List *currying;

    if (!AR_TYPEOF(func, type_function_))
        return ErrorFormat(type_type_error_, "bind expect a function as its first argument, not '%s'",
                           AR_TYPE_NAME(func));

    if (count - 1 > 0) {
        if ((currying = ListNew(count - 1)) == nullptr)
            return nullptr;

        for (ArSize i = 1; i < count; i++)
            ListAppend(currying, argv[i]);

        fnew = FunctionNew(base, currying);
        Release(currying);
        return fnew;
    }

    return IncRef(func);
}

ARGON_FUNCTION(callable,
               "Return true if argument appears callable, false otherwise.\n"
               "\n"
               "- Parameter obj: object to check.\n"
               "- Returns: true if object is callable, false otherwise.",
               1, false) {
    // This definition may be change in future
    if (argv[0]->type == type_function_)
        return True;
    return False;
}

ARGON_FUNCTION(dir,
               "Returns a list of names in the local scope or the attributes of the instance.\n"
               "\n"
               "Without arguments, returns a list with names in the current scope, with one argument, returns a list "
               "with the instance attributes of the argument.\n"
               "\n"
               "- Parameter ...obj: object whose instance attributes you want to know.\n"
               "- Returns: list with attributes if any, otherwise an empty list.",
               0, true) {
    ArObject *ret;
    Frame *frame;

    if (!VariadicCheckPositional("dir", (int) count, 0, 1))
        return nullptr;

    if (count > 0 && AR_TYPEOF(argv[0], type_module_))
        return NamespaceMkInfo(((Module *) argv[0])->module_ns, PropertyType::PUBLIC, false);

    frame = argon::vm::GetFrame();

    if (count == 0) {
        if (frame == nullptr)
            return ListNew();

        if (frame->instance != nullptr)
            ret = NamespaceMkInfo((Namespace *) AR_GET_TYPE(frame->instance)->tp_map, PropertyType{}, true);
        else
            ret = NamespaceMkInfo(frame->globals, PropertyType{}, false);

        Release(frame);
        return ret;
    }

    if (frame != nullptr && frame->instance != nullptr && AR_GET_TYPE(frame->instance) == AR_GET_TYPE(argv[0]))
        ret = NamespaceMkInfo((Namespace *) AR_GET_TYPE(argv[0])->tp_map, PropertyType{}, true);
    else
        ret = NamespaceMkInfo((Namespace *) AR_GET_TYPE(argv[0])->tp_map, PropertyType::PUBLIC, true);

    Release(frame);

    return ret;
}

ARGON_FUNCTION(exit,
               "Close STDIN and starts panicking state with RuntimeExit error.\n"
               "\n"
               "This is a convenient function to terminate your interactive session.\n"
               "\n"
               "- Returns: this function does not return to the caller.",
               0, false) {
    auto *in = (io::File *) argon::vm::ContextRuntimeGetProperty("stdin", io::type_file_);

    if (in != nullptr) {
        io::Close(in);

        // If fail, just ignore it and move on
        if (argon::vm::IsPanicking())
            Release(argon::vm::GetLastError());
    }

    Release(in);
    return argon::vm::Panic(ErrorNew(type_runtime_exit_error_, NilVal));
}

ARGON_FUNCTION(hasnext,
               "Return true if the iterator has more elements.\n"
               "\n"
               "- Parameter iterator: iterator object.\n"
               "- Returns: true if the iterator has more elements, false otherwise.",
               1, false) {
    return BoolToArBool(AR_ITERATOR_SLOT(*argv)->has_next(*argv));
}

ARGON_FUNCTION(input,
               "Allowing user input.\n"
               "\n"
               "- Parameter prompt: string representing a default message before the input.\n"
               "- Returns: string containing user input.", 1, false) {
    StringBuilder builder;
    auto in = (io::File *) argon::vm::ContextRuntimeGetProperty("stdin", io::type_file_);
    auto out = (io::File *) argon::vm::ContextRuntimeGetProperty("stdout", io::type_file_);
    unsigned char *line = nullptr;
    ArSSize len;

    if (in == nullptr || out == nullptr)
        goto ERROR;

    if (io::WriteObjectStr(out, argv[0]) < 0)
        goto ERROR;

    io::Flush(out);
    Release(out);

    if ((len = io::ReadLine(in, &line, -1)) < 0)
        goto ERROR;

    Release(in);

    // len - 1: ignore \n
    builder.Write(line, len - 1, 0);
    argon::memory::Free(line);

    return builder.BuildString();

    ERROR:
    Release(in);
    Release(out);
    return nullptr;
}

ARGON_FUNCTION(isbufferable,
               "Check if object is bufferable.\n"
               "\n"
               "- Parameters:\n"
               "    - obj: object to check.\n"
               "- Returns: true if the object is bufferable, false otherwise.", 1, false) {
    return BoolToArBool(IsBufferable(*argv));
}

ARGON_FUNCTION(isimpl,
               "Check if object implements all the indicated traits.\n"
               "\n"
               "- Parameters:\n"
               "    - obj: object to check.\n"
               "    - ...traits: traits list.\n"
               "- Returns: true if the object implements ALL indicated traits, false otherwise.", 2, true) {
    for (ArSize i = 1; i < count; i++) {
        if (!TraitIsImplemented(argv[0], (TypeInfo *) argv[i]))
            return BoolToArBool(false);
    }

    return BoolToArBool(true);
}

ARGON_FUNCTION(isinstance,
               "Check if object is an instance of indicated type.\n"
               "\n"
               "    - obj: object to check.\n"
               "    - ...types: types list.\n"
               "- Returns: true if the object is an instance of one of the indicated type, false otherwise.", 2, true) {
    for (ArSize i = 1; i < count; i++) {
        if (argv[0]->type == argv[i])
            return BoolToArBool(true);
    }

    return BoolToArBool(false);
}

ARGON_FUNCTION(isiterable,
               "Check if object is iterable.\n"
               "\n"
               "- Parameters:\n"
               "    - obj: object to check.\n"
               "- Returns: true if the object is iterable, false otherwise.", 1, false) {
    return BoolToArBool(IsIterable(*argv));
}

ARGON_FUNCTION(iter,
               "Return an iterator object.\n"
               "\n"
               "- Parameter obj: iterable object.\n"
               "- Returns: new iterator.\n"
               "- Panic TypeError: object is not iterable.\n"
               "\n"
               "# SEE\n"
               "- riter: to obtain a reverse iterator.",
               1, false) {
    return IteratorGet(*argv);
}

ARGON_FUNCTION(len,
               "Returns the length of an object.\n"
               "\n"
               "- Parameter obj: object to check.\n"
               "- Returns: the length of the object.\n"
               "- Panics:\n"
               "  - TypeError: object has no len.\n"
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

ARGON_FUNCTION(lsattr,
               "Returns the list of attributes of an object/datatype.\n"
               "\n"
               "- Parameter obj: object/datatype whose attributes you want to know.\n"
               "- Returns: list with attributes if any, otherwise an empty list.",
               1, false) {
    const auto target = AR_GET_TYPEOBJ(argv[0]);
    ArObject *ret;
    Frame *frame;

    frame = argon::vm::GetFrame();

    if (frame != nullptr && frame->instance != nullptr && AR_GET_TYPE(frame->instance) == target)
        ret = NamespaceMkInfo((Namespace *) target->tp_map, PropertyType{}, true);
    else
        ret = NamespaceMkInfo((Namespace *) target->tp_map, PropertyType::PUBLIC, true);

    Release(frame);

    return ret;
}

ARGON_FUNCTION(next,
               "Retrieve the next item from the iterator.\n"
               "\n"
               "- Parameter iterator: iterator object.\n"
               "- Returns: object.\n"
               "- Panics:\n"
               "     - TypeError: invalid iterator.\n"
               "     - ExhaustedIteratorError: reached the end of the collection.",
               1, false) {
    ArObject *ret = IteratorNext(*argv);

    if (ret == nullptr)
        return ErrorFormat(type_exhausted_iterator_, "reached the end of the collection");

    return ret;
}

ARGON_FUNCTION(recover,
               "Allows a program to manage behavior of panicking ArRoutine.\n"
               "\n"
               "Executing a call to recover inside a deferred function stops "
               "the panicking sequence by restoring normal execution flow.\n"
               "After that the function retrieve and returns the error value passed "
               "to the call of function panic.\n"
               "\n"
               "# WARNING\n"
               "Calling this function outside of deferred function has no effect.\n"
               "\n"
               "- Returns: argument supplied to panic call, or nil if ArRoutine is not panicking.",
               0, false) {
    return ReturnNil(argon::vm::GetLastError());
}

ARGON_FUNCTION(returns,
               "Set and/or get the return value of the function that invoked a defer.\n"
               "\n"
               "If returns is called with:\n"
               "    * 0 argument: no value is set as a return value.\n"
               "    * 1 argument: argument is set as a return value.\n"
               "    * n arguments: the return value is a tuple containing all the passed values.\n"
               "\n"
               "In any case, the current return value is returned.\n"
               "\n"
               "- Parameters:\n"
               "    - ...objs: return value.\n"
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
               "Return an reverse iterator object.\n"
               "\n"
               "- Parameter obj: iterable object.\n"
               "- Returns: new reverse iterator.\n"
               "- Panic TypeError: object is not iterable.\n"
               "\n"
               "# SEE\n"
               "- iter: to obtain an iterator.",
               1, false) {
    return IteratorGetReversed(*argv);
}

ARGON_FUNCTION(peek,
               "Peek item from the iterator.\n"
               "\n"
               "- Parameter iterator: iterator object.\n"
               "- Returns: object.\n"
               "- Panics:\n"
               "     - TypeError: invalid iterator.\n"
               "     - ExhaustedIteratorError: reached the end of the collection.",
               1, false) {
    ArObject *ret = IteratorPeek(*argv);

    if (ret == nullptr)
        return ErrorFormat(type_exhausted_iterator_, "reached the end of the collection");

    return ret;
}

ARGON_FUNCTION(type,
               "Returns type of the object passed as parameter.\n"
               "\n"
               "- Parameter obj: object to get the type from.\n"
               "- Returns: obj type.",
               1, false) {
    IncRef((ArObject *) argv[0]->type);
    return (ArObject *) argv[0]->type;
}

ARGON_FUNCTION(print,
               "Print objects to the stdout, separated by space.\n"
               "\n"
               "- Parameters:\n"
               "     - ...obj: objects to print.\n"
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
               "Same as print, but add new-line at the end.\n"
               "\n"
               "- Parameters:\n"
               "     - ...obj: objects to print.\n"
               "- Returns: nil\n"
               "\n"
               "# SEE\n"
               "- print.",
               0, true) {
    ArObject *success = ARGON_CALL_FUNC(print, func, self, argv, count);

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
        MODULE_EXPORT_TYPE(type_atom_),
        MODULE_EXPORT_TYPE(type_bool_),
        MODULE_EXPORT_TYPE(type_bounds_),
        MODULE_EXPORT_TYPE(type_bytes_),
        MODULE_EXPORT_TYPE(type_code_),
        MODULE_EXPORT_TYPE(type_decimal_),
        MODULE_EXPORT_TYPE(type_function_),
        MODULE_EXPORT_TYPE(type_integer_),
        MODULE_EXPORT_TYPE(type_list_),
        MODULE_EXPORT_TYPE(type_map_),
        MODULE_EXPORT_TYPE(type_module_),
        MODULE_EXPORT_TYPE(type_namespace_),
        MODULE_EXPORT_TYPE_ALIAS("niltype", type_nil_),
        MODULE_EXPORT_TYPE(type_option_),
        MODULE_EXPORT_TYPE(type_set_),
        MODULE_EXPORT_TYPE(type_string_),
        MODULE_EXPORT_TYPE(type_tuple_),

        // Functions
        MODULE_EXPORT_FUNCTION(bind_),
        MODULE_EXPORT_FUNCTION(callable_),
        MODULE_EXPORT_FUNCTION(dir_),
        MODULE_EXPORT_FUNCTION(exit_),
        MODULE_EXPORT_FUNCTION(input_),
        MODULE_EXPORT_FUNCTION(isbufferable_),
        MODULE_EXPORT_FUNCTION(isimpl_),
        MODULE_EXPORT_FUNCTION(isinstance_),
        MODULE_EXPORT_FUNCTION(isiterable_),
        MODULE_EXPORT_FUNCTION(iter_),
        MODULE_EXPORT_FUNCTION(hasnext_),
        MODULE_EXPORT_FUNCTION(len_),
        MODULE_EXPORT_FUNCTION(lsattr_),
        MODULE_EXPORT_FUNCTION(next_),
        MODULE_EXPORT_FUNCTION(peek_),
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
        builtins_bulk,
        nullptr,
        nullptr
};
const ModuleInit *argon::module::module_builtins_ = &module_builtins;
