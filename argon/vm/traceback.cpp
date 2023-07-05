// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/code.h>

#include <argon/vm/runtime.h>
#include <argon/vm/traceback.h>

using namespace argon::vm;
using namespace argon::vm::datatype;

void argon::vm::TBPrintFrame(FILE *file, Frame *frame) {
    if (frame == nullptr) {
        fprintf(file, "<No Argon Frame>");
        return;
    }

    auto *code = frame->code;
    auto pc_offset = frame->instr_ptr - code->instr;

    fprintf(file, "%s:%d @ pc: %p + (0x%lX)",
            ARGON_RAW_STRING(code->qname),
            code->GetLineMapping(pc_offset),
            code->instr,
            pc_offset);
}

void argon::vm::TBPrintStacktraceFromFrame(FILE *file, Frame *frame) {
    for (Frame *cursor = frame; cursor != nullptr; cursor = cursor->back) {
        TBPrintFrame(file, cursor);

        if (frame->back != nullptr)
            fputc('\n', file);
    }
}

void argon::vm::TBPrintPanics(FILE *file) {
    const struct Panic *last = nullptr;
    const struct Panic *prev = nullptr;
    const struct Panic *cursor;

    auto fiber = GetFiber();

    if (fiber == nullptr || fiber->panic == nullptr)
        return;

    while (prev != fiber->panic) {
        cursor = fiber->panic;

        while (last != cursor) {
            prev = cursor;
            cursor = cursor->panic;
        }

        last = prev;

        auto *value = (String *) Str(prev->object);
        if (value == nullptr)
            fprintf(file, "\npanic: %s\n", ARGON_RAW_STRING((String *) ((Error *) prev->object)->reason));
        else
            fprintf(file, "\npanic: %s\n", ARGON_RAW_STRING(value));

        Release(value);

        fprintf(file, "\nTraceback (most recent call FIRST):\n");

        TBPrintStacktraceFromFrame(file, prev->frame);

        fputc('\n', file);
    }
}