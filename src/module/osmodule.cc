// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <utils/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <direct.h>
#include <process.h>
#include "nt/nt.h"

#else

#include <unistd.h>
#include <sys/stat.h>

#endif

#include <vm/runtime.h>

#include <object/datatype/bool.h>
#include <object/datatype/bytes.h>
#include <object/datatype/error.h>
#include <object/datatype/integer.h>
#include <object/datatype/string.h>
#include <object/datatype/nil.h>

#include "modules.h"

using namespace argon::object;
using namespace argon::module;

#define GET_CSTR(variable, target, fname, pname)                        \
    if (AR_TYPEOF(variable, type_string_))                              \
        (target) = (const char *) ((String *) (variable))->buffer;      \
    else if (AR_TYPEOF(variable, type_bytes_))                          \
        (target) = (const char *) ((Bytes *) (variable))->view.buffer;  \
    else                                                                \
        return ErrorFormat(type_type_error_, #fname " expected str/bytes as" #pname ", not '%s'", AR_TYPE_NAME(variable))

#ifdef _ARGON_PLATFORM_WINDOWS

int setenv(const char *name, const char *value, int overwrite) {
    if (std::getenv(name) == nullptr || overwrite)
        return _putenv_s(name, value);

    return 0;
}

#endif

ARGON_FUNCTION(chdir, "Change the current working directory to path."
                      ""
                      "- Parameter path: new current working directory."
                      "- Returns: on success nil is returned, otherwise returns error object.", 1, false) {
    const char *path;

    GET_CSTR(argv[0], path, chdir, path);

    if (chdir(path) != 0)
        return ErrorNewFromErrno();

    return IncRef(NilVal);
}

ARGON_FUNCTION5(os_, exit, "Exit to the system with specified status, without normal exit processing."
                          ""
                          "- Parameter status: an integer value that defines the exit status."
                          "- Returns: this function does not return to the caller.", 1, false) {
    auto *astatus = (Integer *) argv[0];
    int status = EXIT_FAILURE;

    if (AR_TYPEOF(astatus, type_integer_))
        status = (int) astatus->integer;

    exit(status);
}

ARGON_FUNCTION(getcwd, "Return a string representing the current working directory."
                       ""
                       "- Returns: a string with the current working directory.", 0, false) {
    String *ret;
    char *path;
    char *tmp;

    int len = 200;

    if ((path = (char *) argon::memory::Alloc(len)) == nullptr)
        return argon::vm::Panic(error_out_of_memory);

    while (getcwd(path, len) == nullptr) {
        len += 40;
        if ((tmp = (char *) argon::memory::Realloc(path, len)) == nullptr) {
            argon::memory::Free(path);
            return argon::vm::Panic(error_out_of_memory);
        }
        path = tmp;
    }

    ret = StringNew(path);
    argon::memory::Free(path);

    return ret;
}

ARGON_FUNCTION(getenv, "Return the value of the environment variable key if it exists, or default."
                       ""
                       "- Parameters:"
                       "    - key: environment variable key."
                       "    - default: default value."
                       "- Returns: value of the environment variable key, or default.", 2, false) {
    const char *key;
    char *value;

    GET_CSTR(argv[0], key, getenv, key);

    if ((value = std::getenv(key)) != nullptr)
        return StringNew(value);

    return IncRef(argv[1]);
}

ARGON_FUNCTION(getlogin, "Return the name of the user logged in on the controlling terminal of the process."
                         ""
                         "- Returns: string containing the username, or an error object if it fails.", 0, false) {
#ifdef _ARGON_PLATFORM_WINDOWS
    return nt::GetLogin();
#else
    char *name;

    if ((name = getlogin()) != nullptr)
        return StringNew(name);

    return ErrorNewFromErrno();
#endif
}

ARGON_FUNCTION(getpid, "Returns the process ID (PID) of the calling process."
                       ""
                       "- Returns: process ID (PID).", 0, false) {
    return IntegerNew(getpid());
}

ARGON_FUNCTION(mkdir, "Creates a new directory with the specified name and permission bits."
                      ""
                      "- Parameters:"
                      "     - name: directory name."
                      "     - mode: permission bits(integer)."
                      "- Returns: nil on success, error object otherwise.", 2, false) {
    const char *name;
    int error;

    if (!AR_TYPEOF(argv[1], type_integer_))
        return ErrorFormat(type_type_error_, "mkdir expected integer as mode, not '%s'", AR_TYPE_NAME(argv[1]));

    GET_CSTR(argv[0], name, mkdir, name);

#ifdef _ARGON_PLATFORM_WINDOWS
    // TODO: set mode
    error = _mkdir(name);
#else
    error = mkdir(name, ((Integer *) argv[1])->integer);
#endif

    if (error < 0)
        return ErrorNewFromErrno();

    return IncRef(NilVal);
}

ARGON_FUNCTION(rmdir, "Remove (delete) the directory path."
                      ""
                      "- Parameter name: directory name."
                      "- Returns: nil on success, error object otherwise.", 1, false) {
    const char *name;
    int error;

    GET_CSTR(argv[0], name, mkdir, name);

#ifdef _ARGON_PLATFORM_WINDOWS
    error = _rmdir(name);
#else
    error = rmdir(name);
#endif

    if (error < 0)
        return ErrorNewFromErrno();

    return IncRef(NilVal);
}


ARGON_FUNCTION(setenv, "Add or change the environment variable value."
                       ""
                       "setenv adds the variable name to the environment with the value value, if name does not already exist."
                       "If name does exist in the environment, then its value is changed to value if overwrite is nonzero."
                       ""
                       "- Parameters:"
                       "    - key: environment variable key."
                       "    - value: value to add/change to."
                       "    - overwrite: true to change already existing variable."
                       "- Returns: true on success, false otherwise.", 3, false) {
    const char *key;
    const char *value;

    String *avalue;
    bool ok;

    GET_CSTR(argv[0], key, setenv, key);

    if ((avalue = (String *) ToString(argv[1])) == nullptr)
        return nullptr;

    value = (char *) ((String *) avalue)->buffer;

    ok = setenv(key, value, IsTrue(argv[2])) == 0;
    Release(avalue);

    return BoolToArBool(ok);
}

ARGON_FUNCTION(unsetenv, "Delete the environment variable named key."
                         ""
                         "- Parameter key: environment variable key."
                         "- Returns: true on success, false otherwise.", 1, false) {
    const char *key;
    int success;

    GET_CSTR(argv[0], key, unsetenv, key);

#ifdef _ARGON_PLATFORM_WINDOWS
    success = setenv(key, "", 1);
#else
    success = unsetenv(key);
#endif

    return BoolToArBool(success == 0);
}

#undef GET_CSTR

bool os_init(Module *self) {
    bool ok = false;
    String *sep;

#ifdef _ARGON_PLATFORM_WINDOWS
    sep = StringIntern("\\");
#else
    sep = StringIntern("/");
#endif

    if (sep != nullptr)
        ok = ModuleAddProperty(self, "pathsep", sep, MODULE_ATTRIBUTE_PUB_CONST);

    Release(sep);
    return ok;
}

const PropertyBulk os_bulk[] = {
        MODULE_EXPORT_FUNCTION(chdir_),
        MODULE_EXPORT_FUNCTION(os_exit_),
        MODULE_EXPORT_FUNCTION(getcwd_),
        MODULE_EXPORT_FUNCTION(getenv_),
        MODULE_EXPORT_FUNCTION(getlogin_),
        MODULE_EXPORT_FUNCTION(getpid_),
        MODULE_EXPORT_FUNCTION(mkdir_),
        MODULE_EXPORT_FUNCTION(rmdir_),
        MODULE_EXPORT_FUNCTION(setenv_),
        MODULE_EXPORT_FUNCTION(unsetenv_),
        MODULE_EXPORT_SENTINEL
};

const ModuleInit module_os = {
        "os",
        "The module os provides a platform-independent interface to operating system functionality.",
        os_bulk,
        os_init,
        nullptr
};
const ModuleInit *argon::module::module_os_ = &module_os;
