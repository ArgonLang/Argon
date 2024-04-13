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

        /// Code name.
        String *name;

        /// Code qualified name.
        String *qname;

        /// Code documentation.
        String *doc;

        /// Static resources.
        Tuple *statics;

        /// External variables (global scope).
        Tuple *names;

        /// Local variables names (function parameters)
        Tuple *lnames;

        /// Closure.
        Tuple *enclosed;

        /// Array that contains Argon assembly.
        const unsigned char *instr;

        /// Pointer to the end of the array that contains the Argon assembly.
        const unsigned char *instr_end;

        /// Array that contains mapping between code lines and opcodes.
        const unsigned char *linfo;

        /// Length of instr.
        unsigned int instr_sz;

        /// Length of linfo
        unsigned int linfo_sz;

        /// Maximum stack size required to run this code.
        unsigned short stack_sz;

        /// Maximum stack size reserved for local variables.
        unsigned short locals_sz;

        /// Maximum size required by sync stack.
        unsigned short sstack_sz;

        /// Hash value computed on buffer instr.
        ArSize hash;

        /**
         * @brief Set bytecode to code object.
         *
         * @param co_instr Buffer containing the bytecode of the Argon VM (give buffer ownership to the Code object).
         * @param co_instr_sz Length of instr buffer.
         * @param co_stack_sz Length of evaluation stack.
         * @param co_sstack_sz Length of sync stack.
         * @return this
         */
        Code *SetBytecode(const unsigned char *co_instr, unsigned int co_instr_sz,
                          unsigned int co_stack_sz, unsigned int co_sstack_sz) {
            assert(this->instr == nullptr);
            this->instr = co_instr;

            assert(this->instr_end == nullptr);
            this->instr_end = co_instr + co_instr_sz;

            this->instr_sz = co_instr_sz;
            this->sstack_sz = co_sstack_sz;
            this->stack_sz = co_stack_sz;

            return this;
        }

        /***
         * @brief Set mapping between lines and opcodes.
         * @param co_linfo Buffer containing lineno offsets
         * @param size Length of lnotab buffer
         * @return this
         */
        Code *SetTracingInfo(const unsigned char *co_linfo, unsigned int size) {
            assert(this->linfo == nullptr);
            this->linfo = co_linfo;
            this->linfo_sz = size;

            return this;
        }

        /**
         * @brief Set information to code object.
         *
         * @param co_name Code name.
         * @param co_qname Code qualified name.
         * @param co_doc Code Documentation (Makes sense for struct, trait, function, module).
         * @return this
         */
        Code *SetInfo(String *co_name, String *co_qname, String *co_doc) {
            assert(this->name == nullptr);
            this->name = IncRef(co_name);

            assert(this->qname == nullptr);
            this->qname = IncRef(co_qname);

            assert(this->doc == nullptr);
            this->doc = IncRef(co_doc);

            return this;
        }

        unsigned int GetLineMapping(ArSize offset) const;
    };

    _ARGONAPI extern const TypeInfo *type_code_;

    /**
     * @brief Create a new code object.
     *
     * @param statics List of static resources (Int, String, etc...).
     * @param names Global names.
     * @param lnames Local variables.
     * @param enclosed Enclosed variable names (If present, this code is associate to function closure).
     * @param locals_sz Space reserved for local variables on the stack.
     * @return A pointer to an code object, otherwise nullptr.
     */

    Code *CodeNew(List *statics, List *names, List *lnames, List *enclosed, unsigned short locals_sz);

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
