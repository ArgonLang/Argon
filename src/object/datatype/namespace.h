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
        STATIC = 1,
        CONST = 1 << 1,
        PUBLIC = 1 << 2,
        WEAK = 1 << 3
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

        [[nodiscard]] PropertyType operator&(PropertyType type) {
            return this->flags_ & type;
        }

        [[nodiscard]] bool IsConstant() const {
            return ENUMBITMASK_ISTRUE(this->flags_, PropertyType::CONST);
        };

        [[nodiscard]] bool IsPublic() const {
            return ENUMBITMASK_ISTRUE(this->flags_, PropertyType::PUBLIC);
        }

        [[nodiscard]] bool IsStatic() const {
            return ENUMBITMASK_ISTRUE(this->flags_, PropertyType::STATIC);
        }

        [[nodiscard]] bool IsWeak() const {
            return ENUMBITMASK_ISTRUE(this->flags_, PropertyType::WEAK);
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

    Namespace *NamespaceNew(Namespace *ns, PropertyType ignore);

    ArObject *NamespaceGetValue(Namespace *ns, ArObject *key, PropertyInfo *info);

    bool NamespaceNewSymbol(Namespace *ns, ArObject *key, ArObject *value, PropertyInfo info);

    bool NamespaceNewSymbol(Namespace *ns, const char *key, ArObject *value, PropertyInfo info);

    bool NamespaceSetValue(Namespace *ns, ArObject *key, ArObject *value);

    bool NamespaceContains(Namespace *ns, ArObject *key, PropertyInfo *info);

    bool NamespaceRemove(Namespace *ns, ArObject *key);

    int NamespaceSetPositional(Namespace *ns, ArObject **values, ArSize count);

} // namespace argon::object

#endif // !ARGON_OBJECT_NAMESPACE_H_
