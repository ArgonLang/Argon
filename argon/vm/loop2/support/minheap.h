// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_VM_LOOP_SUPPORT_MINHEAP_H_
#define ARGON_VM_LOOP_SUPPORT_MINHEAP_H_

#include <cassert>

namespace argon::vm::loop2::support {
    template<typename T>
    using heap_less = bool (*)(const T *left, const T *right);

    /**
     * @brief Min Heap.
     *
     * @tparam T Any object that exposes a nested struct named heap, containing the following 3 properties:
     * T * parent, T * left, T * right.
     * @tparam LESS The node comparison function.
     */
    template<typename T, heap_less<T> LESS>
    class MinHeap {
        T *head;
        unsigned int nitems;

        T **GetLink(T *t) {
            T *parent = t->heap.parent;

            if (parent == nullptr)
                return &this->head;

            if (parent->heap.left == t)
                return &parent->heap.left;

            return &parent->heap.right;
        }

        void SetChildrenParent(T *t) const {
            if (t->heap.left != nullptr)
                t->heap.left->heap.parent = t;

            if (t->heap.right != nullptr)
                t->heap.right->heap.parent = t;
        }

        /**
         * @brief Check that the node is less than its parent, if not, fix the heap.
         *
         * @param t Node to check.
         */
        void HeapVerify(T *t) {
            if (t->heap.parent == nullptr || LESS(t->heap.parent, t))
                return;

            do {
                this->SwapNode(t);
            } while (t->heap.parent != nullptr && LESS(t, t->heap.parent));
        }

        /**
         * @brief Swap node with its parent.
         *
         * @param t Node to swap.
        */
        void SwapNode(T *t) {
            //        +------------+       +------------+       +------------+
            // ...--->|Super Parent+------>|   Parent   +------>|   Child    |
            //        +------------+       +------------+       +------------+

            T *left = t->heap.left;
            T *right = t->heap.right;
            T **super_link = this->GetLink(t->heap.parent);

            // Move the parent node to the correct side of the child node 't'
            if (t->heap.parent->heap.left == t) {
                t->heap.left = t->heap.parent;
                t->heap.right = t->heap.parent->heap.right;
            } else {
                t->heap.right = t->heap.parent;
                t->heap.left = t->heap.parent->heap.left;
            }

            // Set parent left/right node
            t->heap.parent->heap.left = left;
            t->heap.parent->heap.right = right;
            this->SetChildrenParent(t->heap.parent);

            // Set new t node parent
            t->heap.parent = t->heap.parent->heap.parent;

            // Set the parent of the left and right nodes to t
            this->SetChildrenParent(t);

            // Set super parent link to t
            *super_link = t;
        }

    public:
        /**
         * @brief Returns the lesser node.
         *
         * @return The lesser node.
         */
        T *PeekMin() {
            return this->head;
        }

        /**
         * @brief Removes and returns the lesser node.
         *
         * @return The lesser node.
         */
        T *PopMin() {
            return this->Remove(this->head);
        }

        /**
         * @brief Remove the node from the min heap.
         *
         * @param t Node to removed.
         * @return The removed node.
         */
        T *Remove(T *t) {
            T **target_parent_link = &this->head;
            T *target = *target_parent_link;
            unsigned long path = 0;
            int node = 0;

            if (this->nitems == 0)
                return nullptr;
            else if (this->nitems == 1) {
                this->head = nullptr;
                this->nitems--;

                // Cleans the removed item
                t->heap.parent = nullptr;
                t->heap.left = nullptr;
                t->heap.right = nullptr;
                return t;
            }

            for (auto i = this->nitems; i >= 2; i /= 2) {
                path = (path << 1) | (i & 1);
                node++;
            }

            while (node-- > 0) {
                if (path & 1) {
                    target_parent_link = &target->heap.right;
                    target = target->heap.right;
                } else {
                    target_parent_link = &target->heap.left;
                    target = target->heap.left;
                }

                path >>= 1;
            }

            // Detach the target from its current position (last node in the heap)
            *target_parent_link = nullptr;

            // Target is the last node inserted in the heap!
            assert(target->heap.left == nullptr);
            assert(target->heap.right == nullptr);

            // Move target node in place of the removed node
            *GetLink(t) = target;
            target->heap.parent = t->heap.parent;

            // Set left and right nodes to target
            target->heap.left = t->heap.left != target ? t->heap.left : nullptr;
            target->heap.right = t->heap.right != target ? t->heap.right : nullptr;
            this->SetChildrenParent(target);

            // Cleans the removed item
            t->heap.parent = nullptr;
            t->heap.left = nullptr;
            t->heap.right = nullptr;

            this->HeapVerify(target);

            this->nitems--;

            // Down check
            while (target->heap.left != nullptr) {
                T *to_check = target->heap.left;

                if (target->heap.right != nullptr && LESS(target->heap.right, target->heap.left))
                    to_check = target->heap.right;

                if (!LESS(to_check, target))
                    break;

                this->SwapNode(to_check);
            }

            return t;
        }

        /**
         * @brief Places the node into the min heap.
         *
         * @param t Node to place.
         */
        void Insert(T *t) {
            T **current = &this->head;
            T *parent = *current;
            unsigned long path = 0;
            int node = 0;

            for (auto i = this->nitems + 1; i >= 2; i /= 2) {
                path = (path << 1) | (i & 1);
                node++;
            }

            while (node-- > 0) {
                parent = *current;

                if (path & 1)
                    current = &((*current)->heap.right);
                else
                    current = &((*current)->heap.left);

                path >>= 1;
            }

            t->heap.parent = parent;
            *current = t;

            this->nitems++;

            this->HeapVerify(t);
        }
    };
} // namespace argon::vm::loop::support

#endif // !ARGON_VM_LOOP_SUPPORT_MINHEAP_H_
