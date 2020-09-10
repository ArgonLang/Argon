// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_CONTEXT_H_
#define ARGON_VM_CONTEXT_H_

#include <iostream>

#include <object/arobject.h>
#include <object/datatype/module.h>

namespace argon::vm {

    class Context {
        argon::object::Module *main;

    public:

        Context();

        argon::object::ArObject *Eval(const std::string &source);

        argon::object::ArObject *Eval(std::string &source);
    };

}

#endif // !ARGON_VM_CONTEXT_H_