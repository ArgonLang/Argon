// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_CONFIG_H_
#define ARGON_VM_CONFIG_H_

#define ARGON_EVAR_PATH       "ARGON_PATH"
#define ARGON_EVAR_UNBUFFERED "ARGON_UNBUFFERED"
#define ARGON_EVAR_STARTUP    "ARGON_STARTUP"
#define ARGON_EVAR_MAXVC      "ARGON_MAXVC"

namespace argon::vm {
    struct Config {
        char **argv;
        int argc;

        bool interactive;
        bool quiet;
        bool stack_trace;
        bool unbuffered;
        int cmd;
        int file;
        int max_vc;
        int max_ost;
        int fiber_ss;
        int fiber_pool;
    };

    extern const Config *kConfigDefault;

    bool ConfigInit(Config *config, int argc, char **argv);
} // namespace argon::vm

#endif // !ARGON_VM_CONFIG_H_
