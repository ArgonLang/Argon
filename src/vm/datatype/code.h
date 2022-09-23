// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_CODE_H_
#define ARGON_VM_DATATYPE_CODE_H_

#include "arobject.h"
#include "list.h"
#include "tuple.h"

namespace argon::vm::datatype {
    struct Code {
        AROBJ_HEAD;

        /* Static resources */
        Tuple *statics;

        /* External variables (global scope) */
        Tuple *names;

        /* Local variables (function/cycle scope) */
        Tuple *locals;

        /* Closure */
        Tuple *enclosed;

        /* Array that contains Argon assembly */
        const unsigned char *instr;

        /* Pointer to the end of the array that contains the Argon assembly */
        const unsigned char *instr_end;

        /* Length of instr */
        unsigned int instr_sz;

        /* Maximum stack size required to run this code */
        unsigned int stack_sz;

        ArSize hash;
    };
    extern const TypeInfo *type_code_;

    Code *CodeNew(const unsigned char *instr,
                  unsigned int instr_sz,
                  unsigned int stack_sz,
                  List *statics,
                  List *names,
                  List *locals,
                  List *enclosed);
}

#endif // !ARGON_VM_DATATYPE_CODE_H_
