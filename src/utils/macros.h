// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_UTILS_MACROS_H_
#define ARGON_UTILS_MACROS_H_

// from <bits/wordsize.h>
#ifndef __WORDSIZE
#if (defined __x86_64__ && !defined __ILP32__) || defined __LP64__
# define __WORDSIZE	64
#else
# define __WORDSIZE	32
#endif
#endif

// WORD SIZE
#if defined _WIN64 || __WORDSIZE == 64
#define _ARGON_ENVIRON 64
#elif defined(_WIN32) || __WORDSIZE == 32
#define _ARGON_ENVIRON 32
#endif

// OS PLATFORM
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define _ARGON_PLATFORM_WINDOWS
#define _ARGON_PLATFORM_NAME "windows"
#elif defined(__APPLE__)
#define _ARGON_PLATFORM_DARWIN
#define _ARGON_PLATFORM_NAME "darwin"
#elif defined(__linux__)
#define _ARGON_PLATFORM_LINUX
#define _ARGON_PLATFORM_NAME "linux"
#elif defined(__unix__)
#define _ARGON_PLATFORM_UNIX
#define _ARGON_PLATFORM_NAME "unix"
#else
#define _ARGON_PLATFORM_NAME "unknown"
#endif

#endif // !ARGON_UTILS_MACROS_H_
