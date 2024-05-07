// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <csignal>

#include <argon/vm/signal.h>

#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/function.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/module.h>
#include <argon/vm/datatype/nil.h>

#include <argon/vm/mod/modules.h>

using namespace argon::vm::datatype;

ARGON_FUNCTION(signal_signal, signal,
               "Set the signal handler for the specified signal number to the given handler function.\n"
               "\n"
               "- Parameters:\n"
               "  - signum: Signal number to set the handler for.\n"
               "  - handler: Function to be called when the signal is received.\n",
               "i: signum, Fn: handler", false, false) {
    auto signum = (int) ((Integer *) args[0])->sint;
    auto *func = (Function *) args[1];

    if (signum < 0) {
        ErrorFormat(kValueError[0], "signum cannot be less than zero");
        return nullptr;
    }

    if (!IsNull((ArObject *) func)) {
        auto arity = func->arity;
        if (func->currying != nullptr)
            arity -= func->currying->length;

        if (arity != 1) {
            ErrorFormat(kValueError[0], "expected a handler that only accepts one parameter");
            return nullptr;
        }

        if (func->IsGenerator()) {
            ErrorFormat(kTypeError[0], kTypeError[7], "signal", ARGON_RAW_STRING(func->qname));
            return nullptr;
        }
    }

    if (!argon::vm::SignalAddHandler(signum, func)) {
        ErrorFormat(kValueError[0], "unknown signal %d", signum);
        return nullptr;
    }

    return (ArObject *) IncRef(Nil);
}

ARGON_FUNCTION(signal_reset, reset,
               "Remove all signal handlers that have been set previously.\n"
               "\n",
               nullptr, false, false) {
    argon::vm::SignalResetHandlers();

    return (ArObject *) IncRef(Nil);
}

bool SignalInit(Module *self) {
#define AddIntConstant(c)                   \
    if(!ModuleAddIntConstant(self, #c, c))  \
        return false

#ifdef SIGHUP
    AddIntConstant(SIGHUP);
#endif

#ifdef SIGINT
    AddIntConstant(SIGINT);
#endif

#ifdef SIGQUIT
    AddIntConstant(SIGQUIT);
#endif

#ifdef SIGILL
    AddIntConstant(SIGILL);
#endif

#ifdef SIGTRAP
    AddIntConstant(SIGTRAP);
#endif

#ifdef SIGABRT
    AddIntConstant(SIGABRT);
#endif

#ifdef SIGIOT
    AddIntConstant(SIGIOT);
#endif

#ifdef SIGBUS
    AddIntConstant(SIGBUS);
#endif

#ifdef SIGFPE
    AddIntConstant(SIGFPE);
#endif

#ifdef SIGKILL
    AddIntConstant(SIGKILL);
#endif

#ifdef SIGUSR1
    AddIntConstant(SIGUSR1);
#endif

#ifdef SIGSEGV
    AddIntConstant(SIGSEGV);
#endif

#ifdef SIGUSR2
    AddIntConstant(SIGUSR2);
#endif

#ifdef SIGPIPE
    AddIntConstant(SIGPIPE);
#endif

#ifdef SIGALRM
    AddIntConstant(SIGALRM);
#endif

#ifdef SIGTERM
    AddIntConstant(SIGTERM);
#endif

#ifdef SIGSTKFLT
    AddIntConstant(SIGSTKFLT);
#endif

#ifdef SIGCHLD
    AddIntConstant(SIGCHLD);
#endif

#ifdef SIGCONT
    AddIntConstant(SIGCONT);
#endif

#ifdef SIGSTOP
    AddIntConstant(SIGSTOP);
#endif

#ifdef SIGTSTP
    AddIntConstant(SIGTSTP);
#endif

#ifdef SIGTTIN
    AddIntConstant(SIGTTIN);
#endif

#ifdef SIGTTOU
    AddIntConstant(SIGTTOU);
#endif

#ifdef SIGURG
    AddIntConstant(SIGURG);
#endif

#ifdef SIGXCPU
    AddIntConstant(SIGXCPU);
#endif

#ifdef SIGXFSZ
    AddIntConstant(SIGXFSZ);
#endif

#ifdef SIGVTALRM
    AddIntConstant(SIGVTALRM);
#endif

#ifdef SIGPROF
    AddIntConstant(SIGPROF);
#endif

#ifdef SIGWINCH
    AddIntConstant(SIGWINCH);
#endif

#ifdef SIGIO
    AddIntConstant(SIGIO);
#endif

#ifdef SIGPOLL
    AddIntConstant(SIGPOLL);
#endif

#ifdef SIGPWR
    AddIntConstant(SIGPWR);
#endif

#ifdef SIGSYS
    AddIntConstant(SIGSYS);
#endif

#ifdef SIGUNUSED
    AddIntConstant(SIGUNUSED);
#endif

    return true;
#undef AddIntConstant
}

const ModuleEntry signal_entries[] = {
        MODULE_EXPORT_FUNCTION(signal_signal),
        MODULE_EXPORT_FUNCTION(signal_reset),
        ARGON_MODULE_SENTINEL
};

constexpr ModuleInit ModuleSignal = {
        "signal",
        "This module provides mechanisms to use signal handlers in Argon.",
        nullptr,
        signal_entries,
        SignalInit,
        nullptr
};
const ModuleInit *argon::vm::mod::module_signal_ = &ModuleSignal;
