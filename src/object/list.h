// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_LIST_H_
#define ARGON_OBJECT_LIST_H_

#include "object.h"

#define ARGON_OBJECT_LIST_INITIAL_CAP   4

namespace argon::object {
    class List : public Object {
        Object **objects_;
        size_t cap_;
        size_t len_;

        void CheckSize();

    public:
        List();

        ~List() override;

        explicit List(size_t capacity);

        void Append(Object *item);

        Object *At(size_t index);

        void Clear();

        size_t Count();
    };

} // namespace argon::object

#endif // !ARGON_OBJECT_LIST_H_
