// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER2_OPTIMIZER_H_
#define ARGON_LANG_COMPILER2_OPTIMIZER_H_

#include <argon/lang/compiler2/basicblock.h>

namespace argon::lang::compiler2 {
    enum class OptimizationLevel {
        OFF,

        SOFT,
        MEDIUM,
        HARD
    };

    class CodeOptimizer {
        OptimizationLevel level_;

        void OptimizeJMP(BasicBlock *begin);

    public:
        explicit CodeOptimizer(OptimizationLevel level) : level_(level) {}

        bool optimize(BasicBlockSeq &seq);
    };
}

#endif // !ARGON_LANG_COMPILER2_OPTIMIZER_H_
