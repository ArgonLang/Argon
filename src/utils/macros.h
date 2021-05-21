// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_UTILS_MACROS_H_
#define ARGON_UTILS_MACROS_H_

// from <bits/wordsize.h>
#if defined __x86_64__ && !defined __ILP32__
#ifndef __WORDSIZE
# define __WORDSIZE	64
#endif
#else
# define __WORDSIZE	32
#endif

// WORD SIZE
#if defined _WIN64 || __WORDSIZE == 64
#define _ARGON_ENVIRON 64
#define _ARGON_ENVIRON64
#elif defined(_WIN32) || __WORDSIZE == 32
#define _ARGON_ENVIRON 32
#define _ARGON_ENVIRON32
#endif

// OS PLATFORM
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define _ARGON_PLATFORM windows
#define _ARGON_PLATFORM_NAME "windows"
#elif defined(__APPLE__)
#define _ARGON_PLATFORM darwin
#define _ARGON_PLATFORM_NAME "darwin"
#elif defined(__linux__)
#define _ARGON_PLATFORM linux
#define _ARGON_PLATFORM_NAME "linux"
#elif defined(__unix__)
#define _ARGON_PLATFORM unix
#define _ARGON_PLATFORM_NAME "unix"
#else
#define _ARGON_PLATFORM_NAME "unknown"
#endif

#endif //!ARGON_UTILS_MACROS_H_
