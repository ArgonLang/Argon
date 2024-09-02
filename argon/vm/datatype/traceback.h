// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_TRACEBACK_H_
#define ARGON_VM_DATATYPE_TRACEBACK_H_

#include <argon/vm/opcode.h>

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/code.h>
#include <argon/vm/datatype/integer.h>

namespace argon::vm::datatype {
    struct Traceback {
        AROBJ_HEAD;

        Traceback *back;

        ArObject *panic_obj;

        datatype::Code *code;

        IntegerUnderlying lineno;

        IntegerUnderlying pc_offset;
    };
    extern const datatype::TypeInfo *type_traceback_;

    Traceback *TracebackNew(Code *code, IntegerUnderlying lineno, IntegerUnderlying pc_offset);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_TRACEBACK_H_
