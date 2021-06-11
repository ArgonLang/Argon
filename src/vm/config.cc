// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cstring>
#include <cstdio>

#include "version.h"

#include "config.h"

using namespace argon::vm;

// ---------------------------------------------------------------------------------------------

static const char usage_line[] =
        "usage: %s [option] [-c cmd | file] [arg] ...\n";

static const char usage[] =
        "\nOptions and arguments:\n"
        "-c cmd         : program string\n"
        "-h, --help     : print this help message and exit\n"
        "-u             : force the stdout stream to be unbuffered\n"
        "-v, --version  : print Argon version and exit\n";

// ---------------------------------------------------------------------------------------------

struct ReadOpStatus {
    char **argv;
    char *arg_cur;
    char *argument;

    int argc;
    int argc_cur;
};

struct ReadOpLong {
    char *name;
    bool harg;
    char rt;
};

#define BAD_OPT     0xFF
#define FEW_ARGS    0xFE
#define NONOPT      0xFD
#define ISLOPT      0xFC

int ReadOp(ReadOpStatus *status, const char *opts, ReadOpLong *lopt, int llopt, char tr) {
    const char *subopt;
    int ret;

    status->arg_cur = status->argv[status->argc_cur];
    status->argument = nullptr;

    if (status->argc_cur >= status->argc)
        return -1;

    if (*status->arg_cur == tr) {
        status->arg_cur++;
        if (*status->arg_cur == tr && lopt != nullptr) {
            status->arg_cur++;
            for (int i = 0; i < (llopt / sizeof(ReadOpLong)); i++) {
                if (strcmp(lopt[i].name, status->arg_cur) == 0) {
                    ret = lopt[i].rt == 0 ? ISLOPT : lopt[i].rt;
                    status->argc_cur++;

                    if (!lopt[i].harg)
                        return ret;

                    if (status->argc_cur >= status->argc || *status->argv[status->argc_cur] == tr)
                        return FEW_ARGS;

                    status->argument = status->argv[status->argc_cur++];
                    return ret;
                }
            }
            return BAD_OPT;
        }

        ret = (int) ((unsigned char) *status->arg_cur);
        if ((subopt = strchr(opts, *status->arg_cur)) != nullptr) {
            status->argc_cur++;
            if (*++subopt == '!') {
                if (status->argc_cur >= status->argc || *status->argv[status->argc_cur] == tr)
                    return FEW_ARGS;

                status->argument = status->argv[status->argc_cur];
                status->argc_cur++;
            }
            return ret;
        }
        return BAD_OPT;
    }

    status->argument = status->argv[status->argc_cur++];
    return NONOPT;
}

Config config = {
        nullptr,
        0,
        false,
        -1
};
const Config *argon::vm::global_cfg = &config;

void Help(const char *name) {
    printf(usage_line, name);
    puts(usage);
}

void ParseEnvs() {
    // todo: env flags
}

int argon::vm::ConfigInit(int argc, char **argv) {
    ReadOpLong lopt[] = {
            {(char *) "help",    false, 'h'},
            {(char *) "version", false, 'v'}
    };
    ReadOpStatus status = {};
    int ret = 0;

    status.argv = argv + 1;
    status.argc = argc - 1;

    while (ret != -1 && (ret = ReadOp(&status, "c!huv", lopt, sizeof(lopt), '-')) != -1) {
        switch (ret) {
            case 'c':
                config.cmd = status.argc_cur;
                break;
            case 'h':
                Help(*argv);
                return 0;
            case 'u':
                config.unbuffered = true;
                break;
            case 'v':
                printf("Argon %d.%d.%d(%s)\n", AR_MAJOR, AR_MINOR, AR_PATCH, AR_RELEASE_LEVEL);
                return 0;
            case ISLOPT:
                break;
            case BAD_OPT:
                fprintf(stderr, "unrecognized option: %s\n", status.argv[status.argc_cur]);
                return -1;
            case FEW_ARGS:
                fprintf(stderr, "option %s expected an argument\n", status.argv[status.argc_cur - 1]);
                return -1;
            default:
                // NON OPT
                config.argc = argc - status.argc_cur;
                config.argv = argv + status.argc_cur;
                ret = -1;
        }
    }

    ParseEnvs();

    return 1;
}

#undef BAD_OPT
#undef FEW_ARGS
#undef NONOPT
#undef ISLOPT