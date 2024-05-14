// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <csignal>

#include <argon/vm/datatype/nil.h>

#include <argon/vm/runtime.h>
#include <argon/vm/signal.h>

using namespace argon::vm;
using namespace argon::vm::datatype;

static Function **default_handlers = nullptr;
static SigHandler *handlers = nullptr;
static Context *handlers_context = nullptr;

// Prototypes

struct DefaultHandler {
    const FunctionDef *handler;
    int signum;
};

void signal_handler(int);

// EOL

ARGON_FUNCTION(signal_default_sigint, default_sigint, "", "i: signum", false, false) {
    return (ArObject *) IncRef(Nil);
}

bool argon::vm::SignalInit(Context *context) {
    static const DefaultHandler def_handlers[] = {
            {nullptr, SIGINT}
    };

    if (default_handlers != nullptr)
        return true;

    assert(handlers == nullptr);

    if ((default_handlers = (Function **) memory::Calloc(sizeof(Function *) * NSIG)) == nullptr)
        return false;

    for (auto handler: def_handlers) {
        if (handler.handler == nullptr)
            continue;

        if ((default_handlers[handler.signum] = FunctionNew(handler.handler, nullptr, nullptr)) == nullptr)
            goto ERROR;
    }

    if ((handlers = (SigHandler *) memory::Alloc(sizeof(SigHandler) * NSIG)) == nullptr)
        goto ERROR;

    for (int i = 0; i < NSIG; i++) {
        auto *handler = handlers + i;

        handler->handler = IncRef(default_handlers[i]);
        if (handler->handler != nullptr)
            signal(i, signal_handler);

        new(&handler->lock)std::mutex();
    }

    handlers_context = context;

    return true;

    ERROR:
    for (int i = 0; i < NSIG; i++)
        Release(default_handlers[i]);

    memory::Free(default_handlers);

    default_handlers = nullptr;
    handlers = nullptr;

    return false;
}

bool argon::vm::SignalAddHandler(int signum, Function *func) {
    SigHandler *handler = handlers + signum;
    bool cancel = false;

    if (signum < 0 || signum >= NSIG)
        return false;

    if (IsNull((ArObject *) func)) {
        func = default_handlers[signum];
        cancel = func == nullptr;
    }

    std::unique_lock _(handler->lock);

    Release(handler->handler);

    handler->handler = IncRef(func);

    signal(signum, !cancel ? signal_handler : nullptr);

    return true;
}

void argon::vm::SignalResetHandlers() {
    for (int i = 0; i < NSIG; i++)
        SignalAddHandler(i, default_handlers[i]);
}

#ifndef _ARGON_PLATFORM_WINDOWS
void argon::vm::SignalProcMask() {
    sigset_t mask{};

    sigfillset(&mask);

    pthread_sigmask(SIG_SETMASK, &mask, nullptr);
}
#endif

void signal_handler(int signum) {
    ArObject *argv[1];

    auto *handler = handlers + signum;

    std::unique_lock lock(handler->lock);

#ifdef _ARGON_PLATFORM_WINDOWS
    signal(signum, signal_handler);
#endif

    auto *func = IncRef(handler->handler);

    lock.unlock();

    if (func == nullptr)
        return;

    if ((argv[0] = (ArObject *) IntNew(signum)) == nullptr) {
        Release(func);

        return;
    }

    Release(EvalAsync(handlers_context, func, argv, 1, OpCodeCallMode::FASTCALL));

    Release(func);
    Release(argv[0]);
}
