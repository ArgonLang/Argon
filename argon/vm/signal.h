// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_SIGNAL_H_
#define ARGON_VM_SIGNAL_H_

#include <mutex>

#include <argon/util/macros.h>

#include <argon/vm/datatype/function.h>

#include <argon/vm/context.h>

#ifndef NSIG
#define NSIG 32
#endif

namespace argon::vm {
    struct SigHandler {
        std::mutex lock;

        datatype::Function *handler;
    };

    bool SignalInit(Context *context);

    bool SignalAddHandler(int signum, datatype::Function *func);

#ifndef _ARGON_PLATFORM_WINDOWS

    void SignalProcMask();

#else
    inline void SignalProcMask() {return;}
#endif

    void SignalResetHandlers();

} // namespace argon::vm

#endif // !ARGON_VM_SIGNAL_H_
