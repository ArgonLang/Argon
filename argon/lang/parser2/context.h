// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_PARSER2_CONTEXT_H_
#define ARGON_LANG_PARSER2_CONTEXT_H_

namespace argon::lang::parser2 {
    enum class ContextType {
        MODULE,
        STRUCT,
        TRAIT
    };

    constexpr const char *kContextName[] = {"module",
                                            "struct",
                                            "trait"
    };

    struct Context {
        Context *prev;

        ContextType type;

        explicit Context(ContextType type) : prev(nullptr), type(type) {}

        Context(Context *current, ContextType type) : prev(current), type(type) {}
    };
} // namespace argon::lang::parser2

#endif // ARGON_LANG_PARSER2_CONTEXT_H_
