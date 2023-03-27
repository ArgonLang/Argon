// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_DATATYPE_CODE_H_
#define ARGON_VM_DATATYPE_CODE_H_

#include <argon/vm/opcode.h>

#include <argon/vm/datatype/arobject.h>
#include <argon/vm/datatype/arstring.h>
#include <argon/vm/datatype/list.h>
#include <argon/vm/datatype/tuple.h>

namespace argon::vm::datatype {
    struct Code {
        AROBJ_HEAD;

        /// Code documentation
        String *doc;

        /// Static resources
        Tuple *statics;

        /// External variables (global scope)
        Tuple *names;

        /// Local variables (function/cycle scope)
        Tuple *locals;

        /// Closure
        Tuple *enclosed;

        /// Array that contains Argon assembly
        const unsigned char *instr;

        /// Pointer to the end of the array that contains the Argon assembly
        const unsigned char *instr_end;

        /// Length of instr
        unsigned int instr_sz;

        /// Maximum stack size required to run this code
        unsigned int stack_sz;

        /// Hash value computed on buffer instr
        ArSize hash;
    };
    extern const TypeInfo *type_code_;

    /**
     * @brief Create a new code object.
     *
     * @param instr Buffer containing the bytecode of the Argon VM (give buffer ownership to the Code object).
     * @param doc Code Documentation (Makes sense for struct, trait, function, module).
     * @param statics List of static resources (Int, String, etc...).
     * @param names Global names.
     * @param locals Local variables.
     * @param enclosed Enclosed variable names (If present, this code is associate to function closure).
     * @param instr_sz Length of instr buffer.
     * @param stack_sz Length of evaluation stack.
     * @return A pointer to an code object, otherwise nullptr.
     */
    Code *CodeNew(const unsigned char *instr,
                  String *doc,
                  List *statics,
                  List *names,
                  List *locals,
                  List *enclosed,
                  unsigned int instr_sz,
                  unsigned int stack_sz);

    /**
     * @brief Create a new code object to wrap native function.
     *
     * This function creates a code object containing a function call.
     * It literally contains the following bytecode:
     *
     * CALL argc | mode
     * RET
     *
     * The purpose of this code is to wrap a native function to be used in Argon "thread".
     * If you are curious take a look at "FrameWrapFnNew"
     *
     * @param argc Number of call arguments.
     * @param mode Call mode.
     * @return A pointer to an code object, otherwise nullptr.
     */
    Code *CodeWrapFnCall(unsigned short argc, OpCodeCallMode mode);

} // namespace argon::vm::datatype

#endif // !ARGON_VM_DATATYPE_CODE_H_
