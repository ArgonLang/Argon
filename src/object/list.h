// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_LIST_H_
#define ARGON_OBJECT_LIST_H_

#include "object.h"

#define ARGON_OBJECT_LIST_INITIAL_CAP   4

namespace argon::object {
    class List : public Sequence {
        size_t cap_;

        void CheckSize();

    public:
        List();

        ~List() override;

        explicit List(size_t capacity);

        bool EqualTo(const Object *other) override;

        size_t Hash() override;

        void Append(Object *obj) override;

        Object *GetItem(const Object *i) override;

        void Clear();
    };

    inline const TypeInfo type_list_ = {
            .name=(const unsigned char *) "list",
            .size=sizeof(List)
    };

} // namespace argon::object

#endif // !ARGON_OBJECT_LIST_H_
