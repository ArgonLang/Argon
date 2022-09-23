// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "code.h"

using namespace argon::vm::datatype;

const TypeInfo CodeType = {
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

Code *argon::vm::datatype::CodeNew(const unsigned char *instr, unsigned int instr_sz, unsigned int stack_sz,
                                   List *statics, List *names, List *locals, List *enclosed) {
    auto *code = MakeObject<Code>(&CodeType);

    if (code != nullptr) {
        code->instr = instr;
        code->instr_end = instr + instr_sz;
        code->instr_sz = instr_sz;
        code->stack_sz = stack_sz;

        if ((code->statics = TupleNew((ArObject *) statics)) == nullptr)
            Release(code);
        if ((code->names = TupleNew((ArObject *) names)) == nullptr)
            Release(code);
        if ((code->locals = TupleNew((ArObject *) locals)) == nullptr)
            Release(code);
        if ((code->enclosed = TupleNew((ArObject *) enclosed)) == nullptr)
            Release(code);

    }

    return code;
}