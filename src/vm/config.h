// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_CONFIG_H_
#define ARGON_VM_CONFIG_H_

#define ARGON_EVAR_PATH       "ARGONPATH"
#define ARGON_EVAR_UNBUFFERED "ARGONUNBUFFERED"
#define ARGON_EVAR_STARTUP    "ARGONSTARTUP"
#define ARGON_EVAR_MAXVC      "ARGONMAXVC"

namespace argon::vm {
    struct Config {
        char **argv;
        int argc;

        bool interactive;
        bool quiet;
        bool unbuffered;
        int cmd;
        int file;
        int max_vc;
    };

    extern const Config *kConfigDefault;
}

#endif // !ARGON_VM_CONFIG_H_
