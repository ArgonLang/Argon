// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/boolean.h>

#include <argon/vm/datatype/traceback.h>

using namespace argon::vm::datatype;

const MemberDef traceback_members[] = {
        ARGON_MEMBER("back", MemberType::OBJECT, offsetof(Traceback, back), true),
        ARGON_MEMBER("code", MemberType::OBJECT, offsetof(Traceback, code), true),
        ARGON_MEMBER("lineno", MemberType::INT, offsetof(Traceback, lineno), true),
        ARGON_MEMBER("panic", MemberType::OBJECT, offsetof(Traceback, panic_obj), true),
        ARGON_MEMBER("pc_offset", MemberType::INT, offsetof(Traceback, pc_offset), true),

        ARGON_MEMBER_SENTINEL
};

const ObjectSlots traceback_objslot = {
        nullptr,
        traceback_members,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *traceback_compare(const Traceback *self, const ArObject *other, CompareMode mode) {
    auto *o = (const Traceback *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if ((const ArObject *) self == other)
        return BoolToArBool(true);

    return BoolToArBool(self->code && o->code
                        && self->panic_obj == o->panic_obj
                        && self->pc_offset == o->pc_offset
                        && self->lineno == o->lineno);
}

ArObject *traceback_repr(const Traceback *self) {
    return (ArObject *) StringFormat("%s:%d @ pc: %p + (0x%lX)",
                                     ARGON_RAW_STRING(self->code->qname),
                                     self->lineno,
                                     self->code->instr,
                                     self->pc_offset);
}

bool traceback_dtor(Traceback *self) {
    Release(self->back);
    Release(self->panic_obj);
    Release(self->code);

    return true;
}

TypeInfo TracebackType = {
        AROBJ_HEAD_INIT_TYPE,
        "Traceback",
        nullptr,
        nullptr,
        sizeof(Traceback),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) traceback_dtor,
        nullptr,
        nullptr,
        nullptr,
        (CompareOp) traceback_compare,
        (UnaryConstOp) traceback_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &traceback_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_traceback_ = &TracebackType;

Traceback *argon::vm::datatype::TracebackNew(Code *code, IntegerUnderlying lineno, IntegerUnderlying pc_offset) {
    auto *tb = MakeObject<Traceback>(type_traceback_);

    if (tb != nullptr) {
        tb->back = nullptr;

        tb->panic_obj = nullptr;
        tb->code = IncRef(code);
        tb->lineno = lineno;
        tb->pc_offset = pc_offset;
    }

    return tb;
}
