// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_RSM_H_
#define ARGON_VM_RSM_H_

#include <atomic>
#include <shared_mutex>
#include <thread>

namespace argon::vm::sync {
    /**
     * This implementation allows to use a std::shared_mutex with recursive lock when used in unique_lock mode.
     * It is necessary to be able to use intrinsically recursive functions such as the argon::datatype::Equal on objects of type container (dict, list, tuple, etc...)
     *
     * Hey, better solutions are always welcome! Help me! :)
     */
    class RecursiveSharedMutex {
        std::shared_mutex _rwlock{};
        std::atomic<std::thread::id> _id{};
        unsigned long _count{};

    public:
        void lock() {
            auto id = std::this_thread::get_id();

            if (id == this->_id) {
                this->_count++;
                return;
            }

            this->_rwlock.lock();
            this->_id = id;
            this->_count = 1;
        }

        void unlock() {
            if (this->_count > 1) {
                this->_count--;
                return;
            }

            this->_id = std::thread::id();
            this->_count = 0;
            this->_rwlock.unlock();
        }

        void lock_shared() {
            this->_rwlock.lock_shared();
        }

        void unlock_shared() {
            this->_rwlock.unlock_shared();
        }
    };
} // namespace argon::vm::sync

#endif // !ARGON_VM_RSM_H_
