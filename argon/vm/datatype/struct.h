// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_STRUCT_H_
#define ARGON_VM_DATATYPE_STRUCT_H_

#include <argon/vm/opcode.h>

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/namespace.h>

namespace argon::vm::datatype {
    struct Struct {
        AROBJ_HEAD;

        Namespace *ns;
    };

    ArObject *StructTypeNew(const String *name, const String *qname, const String *doc,
                            Namespace *ns, TypeInfo **bases, unsigned int count);

    Struct *StructNew(TypeInfo *type, ArObject **argv, unsigned int argc, OpCodeInitMode mode);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_STRUCT_H_
