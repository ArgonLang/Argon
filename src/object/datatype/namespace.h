// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_NAMESPACE_H_
#define ARGON_OBJECT_NAMESPACE_H_

#include <object/arobject.h>
#include <utils/enum_bitmask.h>

#include "hmap.h"

namespace argon::object {
    enum class PropertyType : unsigned char {
        CONST = 0x01,
        PUBLIC = 0x01 << 1,
        MEMBER = 0x01 << 2,
        WEAK = 0x01 << 3
    };
}

ENUMBITMASK_ENABLE(argon::object::PropertyType);

namespace argon::object {

    class PropertyInfo {
        PropertyType flags_;
    public:
        PropertyInfo() = default;

        explicit PropertyInfo(PropertyType flags) {
            this->flags_ = flags;
        }

        PropertyInfo &operator=(PropertyType flags) {
            this->flags_ = flags;
            return *this;
        }

        [[nodiscard]] bool IsPublic() const {
            return (this->flags_ & PropertyType::PUBLIC) == PropertyType::PUBLIC;
        }

        [[nodiscard]] bool IsConstant() const {
            return (this->flags_ & PropertyType::CONST) == PropertyType::CONST;
        };

        [[nodiscard]] bool IsWeak() const {
            return (this->flags_ & PropertyType::WEAK) == PropertyType::WEAK;
        }

        [[nodiscard]] bool IsMember() const {
            return (this->flags_ & PropertyType::MEMBER) == PropertyType::MEMBER;
        }
    };

    struct NsEntry : HEntry {
        union {
            ArObject *value;
            RefCount weak;
        };
        PropertyInfo info;
    };

    struct Namespace : ArObject {
        HMap hmap;
    };

    extern const TypeInfo type_namespace_;

    Namespace *NamespaceNew();

    bool NamespaceNewSymbol(Namespace *ns, ArObject *key, ArObject *value, PropertyInfo info);

    bool NamespaceNewSymbol(Namespace *ns, const char *key, ArObject *value, PropertyInfo info);

    bool NamespaceSetValue(Namespace *ns, ArObject *key, ArObject *value);

    bool NamespaceContains(Namespace *ns, ArObject *key, PropertyInfo *info);

    ArObject *NamespaceGetValue(Namespace *ns, ArObject *key, PropertyInfo *info);

    bool NamespaceRemove(Namespace *ns, ArObject *key);

} // namespace argon::object

#endif // !ARGON_OBJECT_NAMESPACE_H_
