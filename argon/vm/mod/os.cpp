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

ARGON_FUNCTION(os_exit, exit,
               "Exit to the system with specified status, without normal exit processing.\n"
               "\n"
               "- Parameter status: Integer value that defines the exit status.\n"
               "- Returns: This function does not return to the caller.\n",
               "i: status", false, false) {
    exit((int) ((Integer *) args[0])->sint);
}

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
        MODULE_EXPORT_FUNCTION(os_exit),
        MODULE_EXPORT_FUNCTION(os_chdir),
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