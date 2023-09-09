// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0
// M.G <3

#include <argon/vm/sync/rsm.h>

#include <argon/util/macros.h>

#if defined(_ARGON_PLATFORM_WINDOWS)
#include <Windows.h>

#define OS_WAIT(ptr, value)                                         \
    do {                                                            \
        auto stored = value;                                        \
        WaitOnAddress(ptr, &stored, sizeof(MutexWord), INFINITE);   \
    } while(0)

#define OS_WAKE(ptr)            WakeByAddressSingle(ptr)

#elif defined(_ARGON_PLATFORM_DARWIN)
extern "C" int __ulock_wait(unsigned int operation, void *addr, unsigned long long value, unsigned int timeout);
extern "C" int __ulock_wake(unsigned int operation, void *addr, unsigned long long wake_value);

#define UL_COMPARE_AND_WAIT 1

#define OS_WAIT(ptr, value)     __ulock_wait(UL_COMPARE_AND_WAIT, ptr, value, 0)
#define OS_WAKE(ptr)            __ulock_wake(UL_COMPARE_AND_WAIT, ptr, 0)

#elif defined(_ARGON_PLATFORM_LINUX)

#include <linux/futex.h>
#include <syscall.h>
#include <unistd.h>

#define futex_wait(ptr, value)  syscall(SYS_futex, ptr, FUTEX_WAIT, value, nullptr, nullptr, 0)
#define futex_wake(ptr)         syscall(SYS_futex, ptr, FUTEX_WAKE, 1, nullptr, nullptr, 0)
// #define futex_wakeall(ptr)   syscall(SYS_futex, ptr, FUTEX_WAKE, INT_MAX, nullptr, nullptr, 0)

#define OS_WAIT(ptr, value)     futex_wait(ptr, value)
#define OS_WAKE(ptr)            futex_wake(ptr)

#else
#error "unsupported platform"
#endif

using namespace argon::vm::sync;

// PRIVATE
void RecursiveSharedMutex::lock_shared_slow(std::thread::id id) {
    auto current = this->_lock.load(std::memory_order_relaxed);
    MutexBits desired{};

    do {
        while (current.is_ulocked() && this->_id != id) {
            OS_WAIT(&this->_lock, current.value());

            current = this->_lock.load(std::memory_order_relaxed);
        }

        desired = current;

        desired.inc_shared();
    } while (!this->_lock.compare_exchange_strong(current, desired));
}

void RecursiveSharedMutex::lock_slow() {
    auto current = MutexBits{};
    auto desired = MutexBits{};

    desired.acquire_unique();

    while (!this->_lock.compare_exchange_strong(current, desired)) {
        OS_WAIT(&this->_lock, current.value());

        current = MutexBits{};
    }
}

// PUBLIC
void RecursiveSharedMutex::lock() {
    auto id = std::this_thread::get_id();

    if (id == this->_id) {
        this->_r_count++;
        return;
    }

    auto current = MutexBits{};
    auto desired = MutexBits{};

    int attempts = 10;

    desired.acquire_unique();

    while (!this->_lock.compare_exchange_strong(current, desired)) {
        if (attempts == 0) {
            this->lock_slow();
            break;
        }

        current = MutexBits{};
        attempts--;
    }

    this->_id = id;
    this->_r_count = 1;
}

void RecursiveSharedMutex::lock_shared() {
    auto id = std::this_thread::get_id();
    auto current = this->_lock.load(std::memory_order_relaxed);
    MutexBits desired{};

    do {
        if (current.is_ulocked() && this->_id != id) {
            this->lock_shared_slow(id);
            return;
        }

        desired = current;
        desired.inc_shared();
    } while (!this->_lock.compare_exchange_strong(current, desired));
}

void RecursiveSharedMutex::unlock() {
    if (this->_r_count > 1) {
        assert(this->_id == std::this_thread::get_id());

        this->_r_count--;
    }

    auto current = this->_lock.load(std::memory_order::memory_order_relaxed);
    MutexBits desired{};

    this->_id = std::thread::id();
    this->_r_count = 0;

    do {
        desired = current;
        desired.release_unique();
    } while (!this->_lock.compare_exchange_strong(current, desired));

    OS_WAKE(&this->_lock);
}

void RecursiveSharedMutex::unlock_shared() {
    auto current = this->_lock.load(std::memory_order_relaxed);
    MutexBits desired{};

    do {
        desired = current;

        desired.dec_shared();
    } while (!this->_lock.compare_exchange_strong(current, desired));

    OS_WAKE(&this->_lock);
}
