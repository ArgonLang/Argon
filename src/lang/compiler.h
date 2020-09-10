// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER2_H_
#define ARGON_LANG_COMPILER2_H_

#include <istream>

#include <object/datatype/code.h>
#include <object/datatype/map.h>

namespace argon::lang {

    class Compiler {
        argon::object::Map *statics_globals;

    public:
        Compiler();

        explicit Compiler(argon::object::Map *statics_globals);

        ~Compiler();

        argon::object::Code *Compile(std::istream *source);
    };

} // namespace argon::lang

#endif // !ARGON_LANG_COMPILER2_H_
