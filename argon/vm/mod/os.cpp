// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/util/macros.h>

#ifdef _ARGON_PLATFORM_WINDOWS

#include <direct.h>
#include <corecrt_io.h>
#include <process.h>
#include <Windows.h>

#include <argon/vm/io/fio.h>
#include <argon/vm/support/nt/handle.h>
#include <argon/vm/support/nt/nt.h>

#undef CONST
#undef ERROR

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
#include <argon/vm/datatype/pcheck.h>
#include <argon/vm/datatype/tuple.h>

#include <argon/vm/mod/modules.h>

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

ARGON_FUNCTION(os_dup, dup,
               "Duplicate or reassigns a file descriptor.\n"
               "\n"
               "- Parameter oldfd: File descriptor referring to open file.\n"
               "- Returns: Returns a new file descriptor.\n",
               "i: oldfd", false, true) {
    IntegerUnderlying newfd;
    int result;

    auto oldfd = (int) ((Integer *) args[0])->sint;

    if (!KParamLookupInt((Dict *) kwargs, "newfd", &newfd, -1))
        return nullptr;

    auto robj = IntNew(0);
    if (robj == nullptr)
        return nullptr;

    if (newfd < 0) {
#ifdef _ARGON_PLATFORM_WINDOWS
        result = _dup(oldfd);
#else
        result = dup(oldfd);
#endif
    } else {
#ifdef _ARGON_PLATFORM_WINDOWS
        result = _dup2(oldfd, (int) newfd);
#else
        result = dup2(oldfd, (int) newfd);
#endif
    }

    if (result < 0) {
        Release(robj);

        ErrorFromErrno(errno);

        return nullptr;
    }

    robj->sint = newfd < 0 ? result : newfd;

    return (ArObject *) robj;
}


ARGON_FUNCTION(os_exit, exit,
               "Exit to the system with specified status, without normal exit processing.\n"
               "\n"
               "- Parameter status: Integer value that defines the exit status.\n"
               "- Returns: This function does not return to the caller.\n",
               "i: status", false, false) {
    exit((int) ((Integer *) args[0])->sint);
}

#ifdef _ARGON_PLATFORM_WINDOWS

bool Dict2StringEnv(Dict *object, char **out) {
    char *envs = nullptr;

    ArSize length = 0;
    ArSize index = 0;

    *out = nullptr;

    if (IsNull((ArObject *) object))
        return true;

    auto *iter = IteratorGet((ArObject *) object, false);
    if (iter == nullptr) {
        memory::Free(envs);

        return false;
    }

    ArObject *cursor;
    bool first = true;
    while ((cursor = IteratorNext(iter)) != nullptr) {
        const auto *kv = (Tuple *) cursor;

        auto *key = (String *) Str(kv->objects[0]);
        auto *value = (String *) Str(kv->objects[1]);

        if (key == nullptr || value == nullptr) {
            Release(key);
            Release(value);

            goto ERROR;
        }

        auto required = ARGON_RAW_STRING_LENGTH(key) + ARGON_RAW_STRING_LENGTH(value) + 1;

        if (first) {
            length = (required * 2) + 1;

            envs = (char *) memory::Alloc(length);
            if (envs == nullptr) {
                Release(key);
                Release(value);

                goto ERROR;
            }
        } else if (required >= (length - 1) - index) {
            length += (required * 2);

            auto *tmp = (char *) memory::Realloc(envs, length);
            if (tmp == nullptr) {
                Release(key);
                Release(value);

                goto ERROR;
            }

            envs = tmp;
        }

        memory::MemoryCopy(envs + index, ARGON_RAW_STRING(key), ARGON_RAW_STRING_LENGTH(key));

        index += ARGON_RAW_STRING_LENGTH(key);

        envs[index++] = '=';

        memory::MemoryCopy(envs + index, ARGON_RAW_STRING(value), ARGON_RAW_STRING_LENGTH(value));

        index += ARGON_RAW_STRING_LENGTH(value);

        envs[index++] = '\0';

        Release(cursor);
        first = false;
    }

    Release(iter);

    if (first) {
        envs = (char *) memory::Alloc(2);
        if (envs == nullptr)
            return false;

        envs[index++] = '\0';
    }

    envs[index] = '\0';

    *out = envs;

    return true;

    ERROR:
    Release(cursor);
    Release(iter);

    memory::Free(envs);

    return false;
}

ARGON_FUNCTION(os_createprocess, createprocess,
               "- Parameters:\n"
               "  - file: Name or path of binary executable.\n"
               "  - args: String passed to the new program as its command-line arguments.\n"
               "- KWParameters:\n"
               "  - envs: Dict of key/value string pairs passed to the new program as environment variables.\n"
               "  - dwFlags: Flags that control the priority class and the creation of the process.\n"
               "  - stdin: Standard input handle for the process.\n"
               "  - stdout: Standard output handle for the process.\n"
               "  - stderr: Standard error handle for the process.\n"
               "  - lpTitle:For console processes, this is the title displayed in the title bar if a new console window is created.\n",
               "sn: file, sn: argv", false, true) {
    PROCESS_INFORMATION pinfo{};
    STARTUPINFO sinfo{};
    argon::vm::support::nt::OSHandle *ohandle = nullptr;

    Dict *envs = nullptr;

    const char *name = nullptr;
    char *argv = nullptr;
    char *exec_envs = nullptr;

    String *lptitle = nullptr;

    io::File *in = nullptr;
    io::File *out = nullptr;
    io::File *err = nullptr;

    int ok = -255;
    UIntegerUnderlying flags;

    if (!IsNull(args[0]))
        name = (const char *) ARGON_RAW_STRING((String *) args[0]);

    if (!IsNull(args[1]) && !String2CString((String *) args[1], &argv))
        return nullptr;

    if (!KParamLookupUInt((Dict *) kwargs, "dwFlags", &flags, 0))
        goto ERROR;

    if (!KParamLookup((Dict *) kwargs, "stdin", io::type_file_, (ArObject **) &in, nullptr, true))
        goto ERROR;

    if (!KParamLookup((Dict *) kwargs, "stdout", io::type_file_, (ArObject **) &out, nullptr, true))
        goto ERROR;

    if (!KParamLookup((Dict *) kwargs, "stderr", io::type_file_, (ArObject **) &err, nullptr, true))
        goto ERROR;

    if (!KParamLookupStr((Dict *) kwargs, "lpTitle", &lptitle, nullptr, nullptr))
        goto ERROR;

    // Extract environments variables
    if (kwargs != nullptr && !DictLookup((Dict *) kwargs, (const char *) "envs", (ArObject **) &envs))
        goto ERROR;

    if (envs != nullptr && !AR_TYPEOF(envs, type_dict_) && envs != (Dict *) Nil) {
        ErrorFormat(kTypeError[0], "expected '%s' or nil, got '%s'", type_dict_->name, AR_TYPE_QNAME(envs));

        goto ERROR;
    }

    if (!Dict2StringEnv(envs, &exec_envs))
        goto ERROR;

    // EOL

    if ((ohandle = argon::vm::support::nt::OSHandleNew(INVALID_HANDLE_VALUE)) == nullptr)
        goto ERROR;

    sinfo.cb = sizeof(STARTUPINFO);

    if (lptitle != nullptr)
        sinfo.lpTitle = (char *) ARGON_RAW_STRING(lptitle);

    if (in != nullptr || out != nullptr || err != nullptr)
        sinfo.dwFlags = STARTF_USESTDHANDLES;

    sinfo.hStdInput = in == nullptr ? nullptr : in->handle;
    sinfo.hStdOutput = out == nullptr ? nullptr : out->handle;
    sinfo.hStdError = err == nullptr ? nullptr : err->handle;

    ok = CreateProcess(
            name,
            argv,
            nullptr,
            nullptr,
            true,
            flags,
            exec_envs,
            nullptr,
            &sinfo,
            &pinfo
    );

    ERROR:

    Release(envs);
    Release(in);
    Release(out);
    Release(err);

    memory::Free(argv);

    argon::vm::memory::Free(exec_envs);

    if (ok == -255) {
        Release(ohandle);
        return nullptr;
    }

    if (ok == 0) {
        Release(ohandle);

        ErrorFromWinErr();

        return nullptr;
    }

    CloseHandle(pinfo.hThread);

    ohandle->handle = pinfo.hProcess;

    return (ArObject *) ohandle;
}

#endif

bool Dict2Env(Dict *object, char ***out) {
    ArObject *cursor;
    char **envs;

    int length = 1;
    int index = 0;

    *out = nullptr;

#ifdef _ARGON_PLATFORM_DARWIN
    if (object == nullptr) {
        *out = *_NSGetEnviron();

        return true;
    }
#endif

    if (IsNull((ArObject *) object))
        return true;

    length += (int) AR_SLOT_SUBSCRIPTABLE(object)->length((const ArObject *) object);

    if ((envs = (char **) memory::Alloc(length * sizeof(void *))) == nullptr)
        return false;

    auto *iter = IteratorGet((ArObject *) object, false);
    if (iter == nullptr) {
        memory::Free(envs);

        return false;
    }

    while ((index < length - 1) && (cursor = IteratorNext(iter)) != nullptr) {
        const auto *kv = (Tuple *) cursor;

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

    if (p_name != nullptr && !String2CString(p_name, argv))
        goto ERROR;

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

        Release(envs);

        return nullptr;
    }

    exec_args = Subscript2Argv(args[1], (String *) (p_name ? args[0] : nullptr));
    if (exec_args == nullptr) {
        Release(envs);

        return nullptr;
    }

    if (!Dict2Env(envs, &exec_env))
        goto ERROR;

#ifndef _ARGON_PLATFORM_WINDOWS
    execve((const char *) ARGON_RAW_STRING((String *) args[0]), exec_args, exec_env);
#else
    _execve((const char *) ARGON_RAW_STRING((String *) args[0]), exec_args, exec_env);
#endif

    ErrorFromErrno(errno);

    ERROR:

    for (int i = 0; exec_args[i] != nullptr; i++)
        argon::vm::memory::Free(exec_args[i]);

    argon::vm::memory::Free(exec_args);

    if (!IsNull((ArObject *) envs) && exec_env != nullptr) {
        for (int i = 0; exec_env[i] != nullptr; i++)
            argon::vm::memory::Free(exec_env[i]);

        argon::vm::memory::Free(exec_env);
    }

    Release(envs);

    return nullptr;
}

#ifndef _ARGON_PLATFORM_WINDOWS

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

ARGON_FUNCTION(os_getexitcode, getexitcode,
               "Retrieves the termination status of the specified process.\n"
               "\n"
               "It returns a tuple with the first value the exit code of the process and the second value a boolean "
               "indicating whether the process is finished or not. \n"
               "If the process is not terminated, the first value does not correspond to the real exit code of the process.\n"
               "\n"
               "- Parameter handle: A handle object associated with a process.\n"
               "- Returns: (exit code, still_active)\n"
               "- Remarks: See Windows GetExitCodeProcess function for more details.\n",
               "o: handle", false, false) {
    Tuple *rt;

#ifdef  _ARGON_PLATFORM_WINDOWS
    rt = TupleNew("ub", 0, false);
    if (rt == nullptr)
        return nullptr;

    auto *handle = (argon::vm::support::nt::OSHandle *) args[0];
    unsigned long status;

    if (!AR_TYPEOF(args[0], argon::vm::support::nt::type_oshandle_)) {
        ErrorFormat(kTypeError[0], kTypeError[2], argon::vm::support::nt::type_oshandle_->name, AR_TYPE_QNAME(args[0]));

        return nullptr;
    }

    auto wcode = WaitForSingleObject(handle->handle, 0);
    if (wcode == WAIT_FAILED) {
        Release(rt);

        ErrorFromWinErr();
        return nullptr;
    } else if (wcode == WAIT_TIMEOUT)
        return (ArObject *) rt;

    if (GetExitCodeProcess(handle->handle, &status) == 0) {
        ErrorFromWinErr();

        return nullptr;
    }

    ((Integer *) rt->objects[0])->uint = status;

    Replace(rt->objects + 1, (ArObject *) True);
#else
    assert(false);
#endif

    return (ArObject *) rt;
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

ARGON_FUNCTION(os_terminateprocess, terminateprocess,
               "Terminates the specified process.\n"
               "\n"
               "- Parameter handle: A handle object associated with a process.\n",
               "o: handle", false, false) {
#ifdef _ARGON_PLATFORM_WINDOWS
    auto *handle = (argon::vm::support::nt::OSHandle *) args[0];

    if (!AR_TYPEOF(args[0], argon::vm::support::nt::type_oshandle_)) {
        ErrorFormat(kTypeError[0], kTypeError[2], argon::vm::support::nt::type_oshandle_->name, AR_TYPE_QNAME(args[0]));

        return nullptr;
    }

    if (TerminateProcess(handle->handle, 0) == 0) {
        ErrorFromWinErr();

        return nullptr;
    }

    return (ArObject *) IncRef(Nil);
#else
    assert(false);
#endif

    return nullptr;
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
#ifdef _ARGON_PLATFORM_WINDOWS
        MODULE_EXPORT_TYPE(argon::vm::support::nt::type_oshandle_),
#endif

        MODULE_EXPORT_FUNCTION(os_chdir),
#ifdef _ARGON_PLATFORM_WINDOWS
        MODULE_EXPORT_FUNCTION(os_createprocess),
#endif
        MODULE_EXPORT_FUNCTION(os_dup),
        MODULE_EXPORT_FUNCTION(os_exit),
        MODULE_EXPORT_FUNCTION(os_execve),
#ifndef _ARGON_PLATFORM_WINDOWS
        MODULE_EXPORT_FUNCTION(os_fork),
#endif
        MODULE_EXPORT_FUNCTION(os_getenv),
        MODULE_EXPORT_FUNCTION(os_getexitcode),
        MODULE_EXPORT_FUNCTION(os_getcwd),
        MODULE_EXPORT_FUNCTION(os_getlogin),
        MODULE_EXPORT_FUNCTION(os_getpid),
        MODULE_EXPORT_FUNCTION(os_mkdir),
        MODULE_EXPORT_FUNCTION(os_rmdir),
        MODULE_EXPORT_FUNCTION(os_setenv),
        MODULE_EXPORT_FUNCTION(os_terminateprocess),
        MODULE_EXPORT_FUNCTION(os_unsetenv),
        ARGON_MODULE_SENTINEL
};

bool OSInit(Module *self) {
#define AddIntConstant(c)                   \
    if(!ModuleAddIntConstant(self, #c, c))  \
        return false

#define AddUIntConstant(c)                  \
    if(!ModuleAddUIntConstant(self, #c, c)) \
        return false

    bool ok;
    String *sep;

#ifdef _ARGON_PLATFORM_WINDOWS
    AddUIntConstant(CREATE_BREAKAWAY_FROM_JOB);
    AddUIntConstant(CREATE_DEFAULT_ERROR_MODE);
    AddUIntConstant(CREATE_NEW_CONSOLE);
    AddUIntConstant(CREATE_NEW_PROCESS_GROUP);
    AddUIntConstant(CREATE_NO_WINDOW);
    AddUIntConstant(CREATE_PROTECTED_PROCESS);
    AddUIntConstant(CREATE_PRESERVE_CODE_AUTHZ_LEVEL);
    AddUIntConstant(CREATE_SECURE_PROCESS);
    AddUIntConstant(CREATE_SEPARATE_WOW_VDM);
    AddUIntConstant(CREATE_SUSPENDED);
    AddUIntConstant(CREATE_UNICODE_ENVIRONMENT);
    AddUIntConstant(DEBUG_ONLY_THIS_PROCESS);
    AddUIntConstant(DEBUG_PROCESS);
    AddUIntConstant(DETACHED_PROCESS);
    AddUIntConstant(EXTENDED_STARTUPINFO_PRESENT);
    AddUIntConstant(INHERIT_PARENT_AFFINITY);

    AddUIntConstant((ArSize) INVALID_HANDLE_VALUE);

    AddIntConstant(STILL_ACTIVE);

    if (!ModuleAddIntConstant(self, "TIMEOUT_INFINITE", INFINITE))
        return false;
#endif

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