// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_RWLOCK_H_
#define ARGON_OBJECT_RWLOCK_H_

#include <atomic>

namespace argon::object {
    class SimpleLock{
        std::atomic_bool flag;

    public:
        SimpleLock() : flag(false) {}

        SimpleLock &operator=(bool status) {
            this->flag.store(status);
            return *this;
        }

        void Lock();

        void Unlock();
    };

    class RWLock {
        std::atomic_uint cf;

    public:
        RWLock() : cf(0) {}

        RWLock &operator=(unsigned int counter) {
            this->cf.store(counter);
            return *this;
        }

        void Lock();

        void RLock();

        void RUnlock();

        void Unlock();
    };

    template<typename T>
    class ReadLock {
        T &lock;
    public:
        explicit ReadLock(T &lock) : lock(lock) {
            lock.RLock();
        }

        ~ReadLock() {
            this->lock.RUnlock();
        }
    };

    using RWLockRead = ReadLock<RWLock>;

    template<typename T>
    class WriteLock {
        T &lock;
        bool locked;
    public:
        explicit WriteLock(T &lock) : lock(lock), locked(true) {
            lock.Lock();
        }

        void RelinquishLock() {
            this->lock.Unlock();
            this->locked = false;
        }

        ~WriteLock() {
            if (this->locked)
                this->lock.Unlock();
        }
    };

    using RWLockWrite = WriteLock<RWLock>;
    using UniqueLock = WriteLock<SimpleLock>;

} // namespace argon::object

#endif // !ARGON_OBJECT_RWLOCK_H_
