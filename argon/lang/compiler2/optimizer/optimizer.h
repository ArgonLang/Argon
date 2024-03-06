// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER2_OPTIMIZER_OPTIMIZER_H_
#define ARGON_LANG_COMPILER2_OPTIMIZER_OPTIMIZER_H_

#include <argon/lang/compiler2/transl_unit.h>

#include <argon/lang/compiler2/optimizer/optim_level.h>

namespace argon::lang::compiler2 {
    class CodeOptimizer {
        TranslationUnit *unit_;
        OptimizationLevel level_;

        void OptimizeJMP();

    public:
        explicit CodeOptimizer(TranslationUnit *unit, OptimizationLevel level) : unit_(unit), level_(level) {}

        bool optimize();
    };
}

#endif // !ARGON_LANG_COMPILER2_OPTIMIZER_OPTIMIZER_H_
