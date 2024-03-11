// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <argon/vm/version.h>
#include <argon/vm/config.h>

using namespace argon::vm;

constexpr Config DefaultConfig = {
        nullptr,
        0,

        true,
        false,
        false,
        false,
        false,
        -1,
        -1,
        0,
        -1,
        -1,
        -1,
        2
};
const Config *argon::vm::kConfigDefault = &DefaultConfig;

// ---------------------------------------------------------------------------------------------

static const char usage_line[] =
        "usage: %s [option] [-c cmd | file] [arg] ...\n";

static const char usage[] =
        "\nOptions and arguments:\n"
        "-c cmd         : program string\n"
        "-h, --help     : print this help message and exit\n"
        "-i             : start interactive mode after running script\n"
        "--nogc         : disable garbage collector\n"
        "-O             : set optimization level (0-3 -- 0: disabled, 3: hard)\n"
        "--pst          : print stacktrace\n"
        "-q             : don't print version messages on interactive startup\n"
        "-u             : force the stdout stream to be unbuffered\n"
        "-v, --version  : print Argon version and exit\n";

static const char usage_env[] =
        "\nEnvironment variables:\n"
        "ARGONUBUFFERED : it is equivalent to specifying the -u option.\n"
        "ARGONMAXVC     : value that controls the number of OS threads that can execute Argon code simultaneously.\n"
        "                 The default value of ARGONMAXVC is the number of CPUs visible at startup.\n"
        "ARGONPATH      : augment the default search path for modules. One or more directories separated by "
        #ifdef _ARGON_PLATFORM_WIDNOWS
        "';' "
        #else
        "':' "
        #endif
        "as the shell's PATH.\n"
        "ARGONSTARTUP   : specifies the script that must be run before the interactive prompt is shown for "
        "the first time.\n";

// ---------------------------------------------------------------------------------------------

struct ReadOpStatus {
    char **argv;
    char *arg_cur;
    char *argument;

    int argc;
    int argc_cur;
};

struct ReadOpLong {
    const char *name;
    bool harg;
    char rt;
};

#define BAD_OPT     0xFF
#define FEW_ARGS    0xFE
#define NONOPT      0xFD
#define ISLOPT      0xFC

int ReadOp(ReadOpStatus *status, const char *opts, const ReadOpLong *lopt, int llopt, char tr) {
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
                    ret = lopt[i].rt == -1 ? ISLOPT : lopt[i].rt;
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

void Help(const char *name) {
    printf(usage_line, name);
    puts(usage);
    puts(usage_env);
}

void ParseEnvs(Config *config) {
    const char *tmp;

    if (std::getenv(ARGON_EVAR_UNBUFFERED) != nullptr)
        config->unbuffered = true;

    if ((tmp = std::getenv(ARGON_EVAR_MAXVC)) != nullptr)
        config->max_vc = (int) strtol(tmp, nullptr, 10);
}

bool argon::vm::ConfigInit(Config *config, int argc, char **argv) {
    ReadOpLong lopt[] = {
            {"help",    false, 'h'},
            {"version", false, 'v'},

            {"nogc",    false, 0},
            {"pst",     false, 1}
    };
    ReadOpStatus status = {};

    int ret = 0;
    bool interactive = false;

    status.argv = argv + 1;
    status.argc = argc - 1;

    while (ret != -1 && (ret = ReadOp(&status, "c!hiOquv", lopt, sizeof(lopt), '-')) != -1) {
        switch (ret) {
            case 0: // --nogc
                config->nogc = true;
                break;
            case 1: // --pst
                config->stack_trace = true;
                break;
            case 'c':
                config->cmd = status.argc_cur;
                config->interactive = interactive;
                break;
            case 'h':
                Help(*argv);
                exit(EXIT_SUCCESS);
            case 'i':
                interactive = true;
                break;
            case 'O': {
                auto lvl = *(status.argv[status.argc_cur]) - '0';

                if (lvl < 0 || lvl > 3) {
                    fprintf(stderr, "invalid optimization level. Expected a value between 0 and 3, got: %s\n",
                            status.argv[status.argc_cur]);
                    exit(EXIT_FAILURE);
                }

                config->optim_lvl = lvl;
                status.argc_cur++;
                break;
            }
            case 'q':
                config->quiet = true;
                break;
            case 'u':
                config->unbuffered = true;
                break;
            case 'v':
                printf("Argon %d.%d.%d(%s)\n", AR_MAJOR, AR_MINOR, AR_PATCH, AR_RELEASE_LEVEL);
                exit(EXIT_SUCCESS);
            case ISLOPT:
                break;
            case BAD_OPT:
                fprintf(stderr, "unrecognized option: %s\n", status.argv[status.argc_cur]);
                exit(EXIT_FAILURE);
            case FEW_ARGS:
                fprintf(stderr, "option %s expected an argument\n", status.argv[status.argc_cur - 1]);
                exit(EXIT_FAILURE);
            default:
                // NON OPT
                config->file = status.argc_cur;
                config->interactive = interactive;

                config->argc = argc - status.argc_cur;
                config->argv = argv + status.argc_cur;

                ret = -1;
        }
    }

    ParseEnvs(config);

    return true;
}