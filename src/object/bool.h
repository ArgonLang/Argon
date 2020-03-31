// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_BOOL_H_
#define ARGON_OBJECT_BOOL_H_

#include <new>
#include <memory/memory.h>

#include "object.h"

namespace argon::object {
    class Bool : public Object {
        bool value_;

        bool EqualTo(const Object *other) override;

        size_t Hash() override;

        explicit Bool(bool value);

    public:
        static Bool *False() {
            static Bool false_(false);
            IncStrongRef(&false_);
            return &false_;
        }

        static Bool *True() {
            static Bool true_(true);
            IncStrongRef(&true_);
            return &true_;
        }
    };

    inline const TypeInfo type_bool_ = {
            .name=(const unsigned char *) "bool",
            .size=sizeof(Bool)
    };

} // namespace argon::object

#endif // !ARGON_OBJECT_BOOL_H_

