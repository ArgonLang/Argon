// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_CONTEXT_H_
#define ARGON_VM_CONTEXT_H_

#include <iostream>

#include <object/arobject.h>
#include <object/datatype/module.h>

#include <modules/io/io.h>

#include <vm/import.h>

namespace argon::vm {

    class Context {
    public:
        argon::object::Module *main;
        argon::object::Module *bltins;
        argon::vm::Import *import;

        argon::modules::io::File *stdout;

        Context();

        argon::object::ArObject *Eval(const std::string &source);

        argon::object::ArObject *Eval(std::string &source);
    };

}

#endif // !ARGON_VM_CONTEXT_H_
