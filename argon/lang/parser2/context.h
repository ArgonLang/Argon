// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER2_CONTEXT_H_
#define ARGON_LANG_PARSER2_CONTEXT_H_

#include <argon/vm/datatype/list.h>

namespace argon::lang::parser2 {
    enum class ContextType {
        FUNC,
        IF,
        LOOP,
        MODULE,
        STRUCT,
        SWITCH,
        TRAIT
    };

    constexpr const char *kContextName[] = {"function",
                                            "if",
                                            "loop",
                                            "module",
                                            "struct",
                                            "switch",
                                            "trait"
    };

    struct Context {
        Context *prev{};

        String *doc{};

        ContextType type;

        explicit Context(ContextType type) : type(type) {}

        Context(Context *current, ContextType type) : prev(current), type(type) {}

        ~Context() {
            Release(this->doc);
        }
    };
} // namespace argon::lang::parser2

#endif // ARGON_LANG_PARSER2_CONTEXT_H_
