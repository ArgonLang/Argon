// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_CODE_H_
#define ARGON_VM_DATATYPE_CODE_H_

#include "arobject.h"
#include "arstring.h"
#include "list.h"
#include "tuple.h"

namespace argon::vm::datatype {
    struct Code {
        AROBJ_HEAD;

        /// Code documentation
        String *doc;

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
                  String *doc,
                  List *statics,
                  List *names,
                  List *locals,
                  List *enclosed,
                  unsigned int instr_sz,
                  unsigned int stack_sz);
}

#endif // !ARGON_VM_DATATYPE_CODE_H_
