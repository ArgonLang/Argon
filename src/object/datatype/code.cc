// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include <lang/opcodes.h>

#include <object/arobject.h>
#include "bool.h"
#include "hash_magic.h"
#include "string.h"
#include "code.h"


using namespace argon::memory;
using namespace argon::object;

bool code_is_true(ArObject *self) {
    return true;
}

ArObject *code_compare(Code *self, ArObject *other, CompareMode mode) {
    bool equal = false;

    if (self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self->instr_sz == ((Code *) other)->instr_sz) {
        equal = true;
        for (ArSize i = 0; i < self->instr_sz; i++) {
            if (self->instr[i] != ((Code *) other)->instr[i]) {
                equal = false;
                break;
            }
        }
    }

    return BoolToArBool(equal);
}

ArSize code_hash(Code *self) {
    if (self->hash == 0)
        self->hash = HashBytes(self->instr, self->instr_sz);

    return self->hash;
}

ArObject *code_str(ArObject *self) {
    return StringNewFormat("<code at %p>", self);
}

void code_cleanup(Code *self) {
    Release(self->statics);
    Release(self->names);
    Release(self->locals);
    Release(self->enclosed);
    Free((void *) self->instr);
}

const TypeInfo CodeType = {
        TYPEINFO_STATIC_INIT,
        "code",
        nullptr,
        sizeof(Code),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) code_cleanup,
        nullptr,
        (CompareOp) code_compare,
        code_is_true,
        (SizeTUnaryOp) code_hash,
        (UnaryOp) code_str,
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
const TypeInfo *argon::object::type_code_ = &CodeType;

Code *argon::object::CodeNew(const unsigned char *instr,
                             unsigned int instr_sz,
                             unsigned int stack_sz,
                             List *statics,
                             List *names,
                             List *locals,
                             List *enclosed) {
    auto code = ArObjectNew<Code>(RCType::INLINE, type_code_);

    if (code != nullptr) {
        code->instr = instr;
        code->instr_sz = instr_sz;
        code->stack_sz = stack_sz;

        if ((code->statics = TupleNew(statics)) == nullptr)
            Release(code);
        if ((code->names = TupleNew(names)) == nullptr)
            Release(code);
        if ((code->locals = TupleNew(locals)) == nullptr)
            Release(code);
        if ((code->enclosed = TupleNew(enclosed)) == nullptr)
            Release(code);
    }

    return code;
}

Code *argon::object::CodeNewNativeWrapper(ArObject *func) {
    auto code = ArObjectNew<Code>(RCType::INLINE, type_code_);
    Tuple *statics;

    unsigned char *instr;

    if (code != nullptr) {
        code->statics = nullptr;
        code->names = nullptr;
        code->locals = nullptr;
        code->enclosed = nullptr;

        code->instr = nullptr;

        if ((statics = TupleNew(1)) == nullptr) {
            Release(code);
            return nullptr;
        }

        if ((instr = (unsigned char *) Alloc(9)) == nullptr) {
            Release(statics);
            Release(code);
            return nullptr;
        }

        /*
         * LSTATIC  0
         * CALL     0
         * RET
         */

        // LSTATIC 0
        *((argon::lang::Instr32 *) instr) = (argon::lang::Instr32) (0 << (unsigned char) 8) |
                                            (argon::lang::Instr8) argon::lang::OpCodes::LSTATIC;
        // CALL 0
        *((argon::lang::Instr32 *) (instr + 4)) = ((unsigned int) (0 << (unsigned char) 24)) |
                                                  ((unsigned short) (0 << (unsigned char) 8)) |
                                                  (argon::lang::Instr8) argon::lang::OpCodes::CALL;
        // RET
        instr[8] = (unsigned char) argon::lang::OpCodes::RET;

        TupleInsertAt(statics, 0, func);

        code->statics = statics;

        code->instr = instr;
        code->instr_sz = 9;
        code->stack_sz = 1;
    }

    return code;
}
