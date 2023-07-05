// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_TRACEBACK_H_
#define ARGON_VM_TRACEBACK_H_

#include <argon/vm/frame.h>

namespace argon::vm {
    void TBPrintPanics(FILE *file);

    void TBPrintFrame(FILE *file, Frame *frame);

    void TBPrintStacktraceFromFrame(FILE *file, Frame * frame);

} // namespace argon::vm

#endif // !ARGON_VM_TRACEBACK_H_
