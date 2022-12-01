// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "code.h"

using namespace argon::vm::datatype;

TypeInfo CodeType = {
        AROBJ_HEAD_INIT_TYPE,
        "Code",
        nullptr,
        nullptr,
        sizeof(Code),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_code_ = &CodeType;

Code *argon::vm::datatype::CodeNew(const unsigned char *instr, String *doc, List *statics, List *names,
                                   List *locals, List *enclosed, unsigned int instr_sz, unsigned int stack_sz) {
    auto *code = MakeObject<Code>(&CodeType);

    if (code != nullptr) {
        code->instr = instr;
        code->instr_end = instr + instr_sz;
        code->instr_sz = instr_sz;
        code->stack_sz = stack_sz;

        code->doc = IncRef(doc);

        if ((code->statics = TupleNew((ArObject *) statics)) == nullptr) {
            Release(code);
            return nullptr;
        }

        if ((code->names = TupleNew((ArObject *) names)) == nullptr) {
            Release(code);
            return nullptr;
        }

        if ((code->locals = TupleNew((ArObject *) locals)) == nullptr) {
            Release(code);
            return nullptr;
        }

        if ((code->enclosed = TupleNew((ArObject *) enclosed)) == nullptr) {
            Release(code);
            return nullptr;
        }
    }

    return code;
}

Code *argon::vm::datatype::CodeWrapFnCall(unsigned short argc, OpCodeCallMode mode) {
    auto *code = MakeObject<Code>(&CodeType);
    unsigned short instr_sz = vm::OpCodeOffset[(unsigned short) OpCode::CALL] +
                              vm::OpCodeOffset[(unsigned short) OpCode::RET];

    unsigned char *buf;

    if (code != nullptr) {
        if ((buf = (unsigned char *) memory::Alloc(instr_sz)) == nullptr) {
            Release(code);
            return nullptr;
        }

        code->instr = buf;

        *((Instr32 *) buf) = ((((unsigned char) mode << 16u) | argc) << 8u) | (unsigned char) OpCode::CALL;
        buf += 4;

        *buf = (unsigned char) OpCode::RET;

        code->instr_end = code->instr + instr_sz;
        code->instr_sz = instr_sz;
        code->stack_sz = argc + 1;

        code->doc = nullptr;
        code->statics = nullptr;
        code->names = nullptr;
        code->locals = nullptr;
        code->enclosed = nullptr;
    }

    return code;
}