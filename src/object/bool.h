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

        explicit Bool(bool value) : value_(value) { IncStrongRef(this); }

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

} // namespace argon::object

#endif // !ARGON_OBJECT_BOOL_H_

