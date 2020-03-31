// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_NUMBER_H_
#define ARGON_OBJECT_NUMBER_H_

#include "object.h"

namespace argon::object {
    class Number : public Object {
    protected:
        explicit Number(const TypeInfo *type) : Object(type) {}

    public:
        bool EqualTo(const Object *other) override {
            // TODO: EqualTo
            return false;
        }

        size_t Hash() override { return 0; }
    };
} // namespace argon::object

#endif // !ARGON_OBJECT_NUMBER_H_
