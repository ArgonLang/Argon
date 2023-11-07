// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <direct.h>
#include <process.h>

#include <argon/vm/support/nt/nt.h>

#else

#include <unistd.h>
#include <sys/stat.h>

#ifdef _ARGON_PLATFORM_DARWIN

#include <crt_externs.h>

#endif

#endif

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/bytes.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/module.h>
#include <argon/vm/datatype/namespace.h>
#include <argon/vm/datatype/nil.h>

#include <argon/vm/mod/modules.h>
#include "argon/vm/datatype/pcheck.h"
#include "argon/vm/datatype/tuple.h"

using namespace argon::vm;
using namespace argon::vm::datatype;

#define GET_CSTR(variable, target, fname, pname)                            \
    if (AR_TYPEOF(variable, type_string_))                                  \
        (target) = (const char *) ARGON_RAW_STRING((String *) variable);    \
    else if (AR_TYPEOF(variable, type_bytes_))                              \
        (target) = (const char *) ((Bytes *) (variable))->view.buffer;      \
    else {                                                                  \
        assert(false);                                                      \
    }

#ifdef _ARGON_PLATFORM_WINDOWS

int setenv(const char *name, const char *value, int overwrite) {
    if (std::getenv(name) == nullptr || overwrite)
        return _putenv_s(name, value);

    return 0;
}

#endif

ARGON_FUNCTION(os_chdir, chdir,
               "Change the current working directory to path.\n"
               "\n"
               "- Parameter path: New current working directory.\n",
               "sx: path", false, false) {
    const char *path;

    GET_CSTR(*args, path, chdir, path)

    if (chdir(path) != 0) {
        ErrorFromErrno(errno);
        return nullptr;
    }

    return (ArObject *) IncRef(Nil);
}

ARGON_FUNCTION(os_exit, exit,
               "Exit to the system with specified status, without normal exit processing.\n"
               "\n"
               "- Parameter status: Integer value that defines the exit status.\n"
               "- Returns: This function does not return to the caller.\n",
               "i: status", false, false) {
    exit((int) ((Integer *) args[0])->sint);
}

#ifndef _ARGON_PLATFORM_WINDOWS

bool Dict2Env(Dict *object, char ***out) {
    ArObject *cursor;
    char **envs;

    int length = 1;
    int index = 0;

#ifdef _ARGON_PLATFORM_DARWIN
    if (object == nullptr) {
        *out = nullptr;

        return *_NSGetEnviron();
    }
#endif

    if (IsNull((ArObject *) object)) {
        *out = nullptr;
        return true;
    }

    length += (int) AR_SLOT_SUBSCRIPTABLE(object)->length((const ArObject *) object);

    if ((envs = (char **) memory::Alloc(length * sizeof(void *))) == nullptr)
        return false;

    auto *iter = IteratorGet((ArObject *) object, false);
    if (iter == nullptr) {
        memory::Free(envs);

        return false;
    }

    while ((index < length - 1) && (cursor = IteratorNext(iter)) != nullptr) {
        auto *kv = (Tuple *) cursor;

        auto *key = (String *) Str(kv->objects[0]);
        auto *value = (String *) Str(kv->objects[1]);

        if (key == nullptr || value == nullptr) {
            Release(key);
            Release(value);
            Release(cursor);
            Release(iter);

            goto ERROR;
        }

        auto *variable = (char *) memory::Alloc(ARGON_RAW_STRING_LENGTH(key) + 1 + ARGON_RAW_STRING_LENGTH(value) + 1);
        if (variable == nullptr) {
            Release(key);
            Release(value);
            Release(cursor);
            Release(iter);

            goto ERROR;
        }

        auto *tmp = (char *) memory::MemoryCopy(variable, ARGON_RAW_STRING(key), ARGON_RAW_STRING_LENGTH(key));

        *tmp++ = '=';

        tmp = (char *) memory::MemoryCopy(tmp, ARGON_RAW_STRING(value), ARGON_RAW_STRING_LENGTH(value));

        *tmp = '\0';

        envs[index++] = variable;

        Release(key);
        Release(value);
        Release(cursor);
    }

    envs[index] = nullptr;

    *out = envs;

    return true;

    ERROR:

    for (int i = 0; i < index; i++)
        memory::Free(envs[i]);

    memory::Free(envs);

    return false;
}

char **Subscript2Argv(ArObject *object, String *p_name) {
    String *str;
    char **argv;

    int argc = 1;
    int index = 0;

    if (p_name != nullptr) {
        argc++;
        index++;
    }

    if (!IsNull(object)) {
        argc += (int) AR_SLOT_SUBSCRIPTABLE(object)->length(object);

        argv = (char **) memory::Alloc(argc * sizeof(char *));
        if (argv == nullptr)
            return nullptr;

        auto *iter = IteratorGet(object, false);
        if (iter == nullptr) {
            memory::Free(argv);

            return nullptr;
        }

        ArObject *cursor;
        while ((index < argc - 1) && (cursor = IteratorNext(iter)) != nullptr) {
            str = (String *) Str(cursor);
            if (str == nullptr) {
                Release(cursor);
                Release(iter);

                goto ERROR;
            }

            if (!String2CString(str, argv + index)) {
                Release(str);
                Release(cursor);
                Release(iter);

                goto ERROR;
            }

            index++;

            Release(str);
            Release(cursor);
        }

        Release(iter);
    } else {
        argv = (char **) memory::Alloc(argc * sizeof(char *));
        if (argv == nullptr)
            return nullptr;
    }

    if (p_name != nullptr) {
        if (!String2CString(p_name, argv))
            goto ERROR;
    }

    argv[index] = nullptr;

    return argv;

    ERROR:

    for (int i = 0; i < index; i++)
        memory::Free(argv[i]);

    memory::Free(argv);

    return nullptr;
}

ARGON_FUNCTION(os_execve, execve,
               "Execve execute a new program, replacing the current process.\n"
               "\n"
               "This function do not return!\n"
               "\n"
               "- Parameters:\n"
               "  - file: Must be either a binary executable, or a script starting with shabang (#!).\n"
               "  - args: List or Tuple of strings passed to the new program as its command-line arguments.\n"
               "- KWParameters:\n"
               "  - name: Boolean indicating whether to insert the program name as the first argument of args.\n"
               "  - envs: Dict of key/value string pairs passed to the new program as environment variables.\n",
               "s: file, ltn: args", false, true) {
    char **exec_args;
    char **exec_env;

    Dict *envs = nullptr;

    bool p_name;

    if (!KParamLookupBool((Dict *) kwargs, "name", &p_name, true))
        return nullptr;

    if (kwargs != nullptr && !DictLookup((Dict *) kwargs, (const char *) "envs", (ArObject **) &envs))
        return nullptr;

    if (envs != nullptr && !AR_TYPEOF(envs, type_dict_) && envs != (Dict *) Nil) {
        ErrorFormat(kTypeError[0], "expected '%s' or nil, got '%s'", type_dict_->name, AR_TYPE_QNAME(envs));

        return nullptr;
    }

    exec_args = Subscript2Argv(args[1], (String *) (p_name ? args[0] : nullptr));
    if (exec_args == nullptr)
        return nullptr;

    if (!Dict2Env(envs, &exec_env))
        goto ERROR;

    execve((const char *) ARGON_RAW_STRING((String *) args[0]), exec_args, exec_env);

    ErrorFromErrno(errno);

    ERROR:

    for (int i = 0; exec_args[i] != nullptr; i++)
        argon::vm::memory::Free(exec_args[i]);

    argon::vm::memory::Free(exec_args);

    if (!IsNull((ArObject *) envs)) {
        for (int i = 0; exec_env[i] != nullptr; i++)
            argon::vm::memory::Free(exec_env[i]);

        argon::vm::memory::Free(exec_env);
    }

    return nullptr;
}

ARGON_FUNCTION(os_fork, fork,
               "Creates a new process by duplicating the calling process.\n"
               "\n"
               "- Returns: On success, the PID of the child process is returned in the parent, "
               "and 0 is returned in the child.",
               nullptr, false, false) {
    auto *rvalue = IntNew(0);
    if (rvalue == nullptr)
        return nullptr;

    auto status = fork();

    if (status < 0) {
        Release(rvalue);

        ErrorFromErrno(errno);

        return nullptr;
    }

    rvalue->sint = status;

    return (ArObject *) rvalue;
}

#endif

ARGON_FUNCTION(os_getcwd, getcwd,
               "Return a string representing the current working directory.\n"
               "\n"
               "- Returns: String with the current working directory.\n",
               nullptr, false, false) {
    String *ret;
    char *path;
    char *tmp;

    int len = 200;

    if ((path = (char *) argon::vm::memory::Alloc(len)) == nullptr)
        return nullptr;

    while (getcwd(path, len) == nullptr) {
        len += 40;

        if ((tmp = (char *) argon::vm::memory::Realloc(path, len)) == nullptr) {
            argon::vm::memory::Free(path);
            return nullptr;
        }

        path = tmp;
    }

    ret = StringNew(path);
    argon::vm::memory::Free(path);

    return (ArObject *) ret;
}

ARGON_FUNCTION(os_getenv, getenv,
               "Return the value of the environment variable key if it exists, or default.\n"
               "\n"
               "- Parameters:\n"
               "  - key: Environment variable key.\n"
               "  - default: Default value.\n"
               "- Returns: Value of the environment variable key, or default.\n",
               "sx: key, : value", false, false) {
    const char *key;
    const char *value;

    GET_CSTR(*args, key, getenv, key)

    if ((value = std::getenv(key)) != nullptr)
        return (ArObject *) StringNew(value);

    return IncRef(args[1]);
}

ARGON_FUNCTION(os_getlogin, getlogin,
               "Return the name of the user logged in on the controlling terminal of the process.\n"
               "\n"
               "- Returns: String containing the username.",
               nullptr, false, false) {
#ifdef _ARGON_PLATFORM_WINDOWS
    return argon::vm::support::nt::GetLogin();
#else
    char *name;

    if ((name = getlogin()) != nullptr)
        return (ArObject *) StringNew(name);

    ErrorFromErrno(errno);

    return nullptr;
#endif
}

ARGON_FUNCTION(os_getpid, getpid,
               "Returns the process ID (PID) of the calling process.\n"
               "\n"
               "- Returns: Process ID (PID).\n",
               nullptr, false, false) {
    return (ArObject *) IntNew(getpid());
}

ARGON_FUNCTION(os_mkdir, mkdir,
               "Creates a new directory with the specified name and permission bits.\n"
               "\n"
               "- Parameters:\n"
               "  - name: Directory name.\n"
               "  - mode: Permission bits(integer).\n",
               "sx: key, i: mode", false, false) {
    const char *name;
    int error;

    GET_CSTR(*args, name, mkdir, name)

#ifdef _ARGON_PLATFORM_WINDOWS
    // TODO: set mode
    error = _mkdir(name);
#else
    error = mkdir(name, (unsigned char) ((Integer *) args[1])->sint);
#endif

    if (error < 0) {
        ErrorFromErrno(errno);
        return nullptr;
    }

    return (ArObject *) IncRef(Nil);
}

ARGON_FUNCTION(os_rmdir, rmdir,
               "Remove (delete) the directory path.\n"
               "\n"
               "- Parameter name: Directory name.\n",
               "sx: key", false, false) {
    const char *name;
    int error;

    GET_CSTR(*args, name, mkdir, name)

#ifdef _ARGON_PLATFORM_WINDOWS
    error = _rmdir(name);
#else
    error = rmdir(name);
#endif

    if (error < 0) {
        ErrorFromErrno(errno);
        return nullptr;
    }

    return (ArObject *) IncRef(Nil);
}

ARGON_FUNCTION(os_setenv, setenv,
               "Add or change the environment variable value.\n"
               "\n"
               "setenv adds the variable name to the environment with the value value, if name does not already exist.\n"
               "If name does exist in the environment, then its value is changed to value if overwrite is nonzero.\n"
               "\n"
               "- Parameters:\n"
               "  - key: Environment variable key.\n"
               "  - value: Value to add/change to.\n"
               "  - overwrite: True to change already existing variable.\n"
               "- Returns: True on success, false otherwise.\n",
               "sx: key, : obj, b: overwrite", false, false) {
    const char *key;
    const char *value;

    String *avalue;
    bool ok;

    GET_CSTR(*args, key, setenv, key)

    if ((avalue = (String *) Str(args[1])) == nullptr)
        return nullptr;

    value = (char *) ARGON_RAW_STRING((String *) avalue);

    ok = setenv(key, value, IsTrue(args[2])) == 0;
    Release(avalue);

    return BoolToArBool(ok);
}

ARGON_FUNCTION(os_unsetenv, unsetenv,
               "Delete the environment variable named key.\n"
               "\n"
               "- Parameter key: Environment variable key.\n"
               "- Returns: True on success, false otherwise.\n",
               "sx: key", false, false) {
    const char *key;
    int success;

    GET_CSTR(*args, key, unsetenv, key)

#ifdef _ARGON_PLATFORM_WINDOWS
    success = setenv(key, "", 1);
#else
    success = unsetenv(key);
#endif

    return BoolToArBool(success == 0);
}

const ModuleEntry os_entries[] = {
        MODULE_EXPORT_FUNCTION(os_chdir),
        MODULE_EXPORT_FUNCTION(os_exit),
#ifndef _ARGON_PLATFORM_WINDOWS
        MODULE_EXPORT_FUNCTION(os_execve),
        MODULE_EXPORT_FUNCTION(os_fork),
#endif
        MODULE_EXPORT_FUNCTION(os_getenv),
        MODULE_EXPORT_FUNCTION(os_getcwd),
        MODULE_EXPORT_FUNCTION(os_getlogin),
        MODULE_EXPORT_FUNCTION(os_getpid),
        MODULE_EXPORT_FUNCTION(os_mkdir),
        MODULE_EXPORT_FUNCTION(os_rmdir),
        MODULE_EXPORT_FUNCTION(os_setenv),
        MODULE_EXPORT_FUNCTION(os_unsetenv),

        ARGON_MODULE_SENTINEL
};

bool OSInit(Module *self) {
#define AddIntConstant(c)                   \
    if(!ModuleAddIntConstant(self, #c, c))  \
        return false

    bool ok;
    String *sep;

    AddIntConstant(EXIT_SUCCESS);

    AddIntConstant(EXIT_FAILURE);

    if ((sep = StringIntern(_ARGON_PLATFORM_PATHSEP)) == nullptr)
        return false;

    ok = ModuleAddObject(self, "pathsep", (ArObject *) sep, MODULE_ATTRIBUTE_DEFAULT);

    Release(sep);

    return ok;
}

constexpr ModuleInit ModuleOs = {
        "os",
        "The module os provides a platform-independent interface to operating system functionality.",
        nullptr,
        os_entries,
        OSInit,
        nullptr
};
const ModuleInit *argon::vm::mod::module_os_ = &ModuleOs;