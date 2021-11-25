// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <iostream>

#include <object/datatype/integer.h>
#include <object/datatype/error.h>
#include <object/datatype/nil.h>
#include <object/datatype/io/io.h>

#include <lang/scanner/scanner.h>
#include <lang/compiler_wrapper.h>

#include "areval.h"
#include "config.h"
#include "argon.h"

using namespace argon::object;
using namespace argon::vm;

int ErrorWrapper(bool *must_exit) {
    ArObject *err = GetLastError();
    int exit = EXIT_FAILURE;

    if (must_exit != nullptr)
        *must_exit = false;

    if (AR_TYPEOF(err, type_runtime_exit_error_)) {
        if (must_exit != nullptr)
            *must_exit = true;

        if (((Error *) err)->obj == NilVal) {
            Release(err);
            return EXIT_SUCCESS;
        }

        if (AR_TYPEOF(((Error *) err)->obj, type_integer_)) {
            exit = (int) ((Integer *) ((Error *) err)->obj)->integer;
            Release(err);
            return exit;
        }
    }

    ErrorPrint(err);
    Release(err);
    return exit;
}

void Print(ArObject *obj) {
    auto *out = (io::File *) ContextRuntimeGetProperty("stdout", io::type_file_);
    ArObject *err;
    String *str;

    if (out == nullptr || !AR_TYPEOF(out, io::type_file_)) {
        // Silently discard and continue
        Release(out);
        return;
    }

    if ((str = (String *) ToString(obj)) == nullptr) {
        err = GetLastError();
        ErrorPrint(err);
        Release(err);
        Release(out);
        return;
    }

    if (io::WriteObject(out, str) >= 0)
        io::WriteString(out, "\n");

    Release(str);
}

bool SetupImportBases() {
    ArObject *err;
    String *tmp;
    String *basedir;

    ArSSize last;

    if ((tmp = (String *) ContextRuntimeGetProperty("executable", type_string_)) == nullptr) {
        err = GetLastError();
        ErrorPrint(err);
        Release(err);
        return false;
    }

    if ((last = StringRFind(tmp, GetContext()->import->path_sep)) == -1) {
        Release(tmp);
        return true;
    }

    basedir = StringSubs(tmp, 0, last + 1);

    Release(tmp);

    if (basedir == nullptr || !ImportAddPath(GetContext()->import, basedir)) {
        Release(basedir);
        return false;
    }

    if ((tmp = StringConcat(basedir, "packages", true)) == nullptr) {
        Release(basedir);
        return false;
    }

    Release(basedir);

    if (tmp == nullptr || !ImportAddPath(GetContext()->import, tmp)) {
        Release(tmp);
        return false;
    }

    Release(tmp);
    return true;
}

bool SetUpImportPaths() {
    const char *arpaths = std::getenv(ARGON_ENVVAR_PATH);
    String *path;
    List *paths;

    if (!SetupImportBases())
        return false;

    if (arpaths == nullptr)
        return true;

    if ((path = StringNew(arpaths)) == nullptr)
        return false;

#ifdef _ARGON_PLATFORM_WINDOWS
    if ((paths = (List *) StringSplit(path, ";", -1)) == nullptr) {
        Release(path);
        return false;
    }
#else
    if ((paths = (List *) StringSplit(path, ":", -1)) == nullptr) {
        Release(path);
        return false;
    }
#endif

    Release(path);

    if (!ImportAddPaths(GetContext()->import, paths)) {
        Release(paths);
        return false;
    }

    Release(paths);
    return true;
}

int argon::vm::Main(int argc, char **argv) {
    int exit = EXIT_SUCCESS;

    if (argon::vm::ConfigInit(argc, argv) <= 0)
        return EXIT_FAILURE;

    if (!Initialize())
        return EXIT_FAILURE;

    if (!SetUpImportPaths())
        return EXIT_FAILURE;

    AcquireMain();

    if (global_cfg->file > -1) {
        Release(EvalFile(argv[global_cfg->file]));

        if (IsPanicking())
            return ErrorWrapper(nullptr);
    }

    if (global_cfg->cmd > -1) {
        Release(EvalString(argv[global_cfg->cmd]));

        if (IsPanicking())
            return ErrorWrapper(nullptr);
    }

    ReleaseMain();

    if (global_cfg->interactive)
        exit = EvalInteractive();

    Shutdown();

    return exit;
}

argon::object::ArObject *argon::vm::EvalFile(const char *file) {
    lang::CompilerWrapper cw;
    ArObject *res = nullptr;
    Code *code;
    FILE *f;

    if ((f = fopen(file, "r")) == nullptr)
        return ErrorSetFromErrno();

    if ((code = cw.Compile("main", f)) != nullptr) {
        res = EvalCode(code);
        Release(code);
    }

    fclose(f);
    return res;
}

ArObject *argon::vm::EvalString(const char *str) {
    lang::CompilerWrapper cw;
    ArObject *res = nullptr;
    Code *code;

    if ((code = cw.Compile("main", str)) != nullptr) {
        res = EvalCode(code);
        Release(code);
    }

    return res;
}

ArObject *argon::vm::EvalCode(Code *code, Namespace *globals) {
    ArObject *res = nullptr;
    Frame *frame;

    RoutineReset(GetRoutine(), ArRoutineStatus::RUNNING);

    if ((frame = FrameNew(code, globals, nullptr)) != nullptr)
        res = Eval(GetRoutine(), frame);

    return res;
}

ArObject *argon::vm::EvalCode(Code *code) {
    ArObject *res = nullptr;
    Module *main;

    if ((main = ImportAddModule(GetContext()->import, "main")) != nullptr) {
        res = EvalCode(code, main->module_ns);
        Release(main);
    }

    return res;
}

int argon::vm::EvalInteractive() {
    const char *startup = std::getenv(ARGON_ENVVAR_STARTUP);
    ArObject *ret;

    if (startup != nullptr) {
        Release(EvalFile(startup));

        if (IsPanicking())
            return ErrorWrapper(nullptr);
    }

    if (!global_cfg->quiet) {
        if ((ret = ContextRuntimeGetProperty("version_ex", type_string_)) == nullptr)
            return ErrorWrapper(nullptr);

        Print(ret);
    }

    return StartInteractiveLoop();
}

void GetCPS(ArObject **ps, unsigned char **c_ps) {
    String *tmp;

    if (ps == nullptr || *ps == nullptr || c_ps == nullptr)
        return;

    *c_ps = (unsigned char *) "";

    if (AR_TYPEOF(*ps, type_string_)) {
        *c_ps = ((String *) *ps)->buffer;
        return;
    }

    if ((tmp = (String *) ToString(*ps)) == nullptr)
        return;

    // Silently discard error in string conversion
    if (IsPanicking()) {
        Release(GetLastError()); // TODO: improve
        return;
    }

    Release(ps);
    *ps = tmp;
    *c_ps = tmp->buffer;
}

int argon::vm::StartInteractiveLoop() {
    const unsigned char *c_ps1 = nullptr;
    const unsigned char *c_ps2 = nullptr;

    ArObject *ps1;
    ArObject *ps2;
    ArObject *ret;
    Code *code;

    argon::lang::CompilerWrapper cw;

    bool eof = false;

    int exit;

    while (!eof) {
        ps1 = ContextRuntimeGetProperty("ps1", nullptr);
        GetCPS(&ps1, (unsigned char **) &c_ps1);

        ps2 = ContextRuntimeGetProperty("ps2", nullptr);
        GetCPS(&ps2, (unsigned char **) &c_ps2);

        lang::scanner::Scanner scanner(stdin, (const char *) c_ps1, (const char *) c_ps2);

        code = cw.Compile("stdin", &scanner);

        eof = scanner.status == argon::lang::scanner::ScannerStatus::END_OF_FILE;

        AcquireMain();

        ret = nullptr;
        if (code != nullptr) {
            if ((ret = EvalCode(code)) != nullptr && ret != NilVal)
                Print(ret);
        }

        exit = EXIT_SUCCESS;
        if (code == nullptr || ret == nullptr)
            exit = ErrorWrapper(&eof);

        Release(ret);
        Release(ps1);
        Release(ps2);

        ReleaseMain();
    }

    return exit;
}