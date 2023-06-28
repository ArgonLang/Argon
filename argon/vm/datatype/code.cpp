// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/hash_magic.h>

#include <argon/vm/datatype/code.h>

using namespace argon::vm::datatype;

unsigned int Code::GetLineMapping(ArSize offset) const {
    ArSize mapping_offset = 0;
    unsigned int mapping_line = 0;

    for (unsigned int i = 0; i < this->linfo_sz; i += 2) {
        mapping_offset += this->linfo[i];
        mapping_line += (char) this->linfo[i + 1];
        if (mapping_offset > offset)
            break;
    }

    return mapping_line;
}

ArObject *code_compare(const Code *self, const ArObject *other, CompareMode mode) {
    bool equal = false;

    if ((const ArObject *) self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self->instr_sz == ((const Code *) other)->instr_sz)
        equal = argon::vm::memory::MemoryCompare(self->instr, ((const Code *) other)->instr, self->instr_sz) == 0;

    return BoolToArBool(equal);
}

ArSize code_hash(Code *self) {
    if (self->hash == 0)
        self->hash = HashBytes(self->instr, self->instr_sz);

    return self->hash;
}

bool code_dtor(Code *self) {
    Release(self->name);
    Release(self->qname);
    Release(self->doc);

    Release(self->statics);
    Release(self->names);
    Release(self->locals);
    Release(self->enclosed);

    argon::vm::memory::Free((void *) self->instr);

    return true;
}

TypeInfo CodeType = {
        AROBJ_HEAD_INIT_TYPE,
        "Code",
        nullptr,
        nullptr,
        sizeof(Code),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) code_dtor,
        nullptr,
        (ArSize_UnaryOp) code_hash,
        nullptr,
        (CompareOp) code_compare,
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

Code *argon::vm::datatype::CodeNew(List *statics, List *names, List *locals, List *enclosed) {
    auto *code = MakeObject<Code>(&CodeType);

    if (code != nullptr) {
        code->name = nullptr;
        code->qname = nullptr;
        code->doc = nullptr;

        code->instr = nullptr;
        code->instr_end = nullptr;
        code->linfo = nullptr;

        code->instr_sz = 0;
        code->stack_sz = 0;
        code->linfo_sz = 0;

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

        code->linfo = nullptr;
        code->linfo_sz = 0;

        code->name = nullptr;
        code->qname = nullptr;
        code->doc = nullptr;
        code->statics = nullptr;
        code->names = nullptr;
        code->locals = nullptr;
        code->enclosed = nullptr;
    }

    return code;
}