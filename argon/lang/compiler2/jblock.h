// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER2_JBLOCK_H_
#define ARGON_LANG_COMPILER2_JBLOCK_H_

#include <argon/vm/datatype/arstring.h>

#include <argon/lang/compiler2/basicblock.h>

namespace argon::lang::compiler2 {
    enum class JBlockType {
        LABEL,
        LOOP,
        SAFE,
        SWITCH,
        SYNC,
        TRAP
    };

    struct JBlock {
        JBlock *prev;

        argon::vm::datatype::String *label;

        BasicBlock *begin;
        BasicBlock *end;

        JBlockType type;

        unsigned short pops;
    };

    JBlock *JBlockNew( JBlock *prev, argon::vm::datatype::String *label, JBlockType type);

    JBlock *JBlockDel(JBlock *block);

} // namespace argon::lang::compiler2

#endif // !ARGON_LANG_COMPILER2_JBLOCK_H_
