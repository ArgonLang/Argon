// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_CONFIG_H_
#define ARGON_VM_CONFIG_H_

#include <utils/macros.h>

namespace argon::vm {
    struct Config {
        char **argv;
        int argc;

        bool unbuffered;
        int cmd;
    };

    extern const _ARGONAPI Config *global_cfg;

    int ConfigInit(int argc, char **argv);
}

#endif // !ARGON_VM_CONFIG_H_
