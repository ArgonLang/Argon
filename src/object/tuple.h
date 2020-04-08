// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_TUPLE_H_
#define ARGON_OBJECT_TUPLE_H_

#include "object.h"

namespace argon::object {
    class Tuple : public Sequence {

    public:
        explicit Tuple(const Object *seq);

        ~Tuple() override;

        bool EqualTo(const Object *other) override;

        size_t Hash() override;

        Object *GetItem(const Object *i) override;

        Object *GetItem(size_t i);
    };

    inline const TypeInfo type_tuple_ = {
            .name=(const unsigned char *) "tuple",
            .size=sizeof(Tuple)
    };

} // namespace argon::object

#endif // !ARGON_OBJECT_TUPLE_H_
