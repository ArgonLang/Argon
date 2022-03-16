// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_NAMESPACE_H_
#define ARGON_OBJECT_NAMESPACE_H_

#include <object/arobject.h>
#include <utils/enum_bitmask.h>

#include "nil.h"
#include "hmap.h"

namespace argon::object {
    enum class PropertyType : unsigned char {
        CONST = 1,
        PUBLIC = 1 << 1,
        WEAK = 1 << 2
    };
}

ENUMBITMASK_ENABLE(argon::object::PropertyType);

namespace argon::object {

    class PropertyInfo {
        PropertyType flags_;
    public:
        PropertyInfo() = default;

        PropertyInfo &operator=(PropertyType flags) {
            this->flags_ = flags;
            return *this;
        }

        explicit operator PropertyType() const {
            return this->flags_;
        }

        [[nodiscard]] PropertyType operator&(PropertyType type) const {
            return this->flags_ & type;
        }

        [[nodiscard]] bool IsConstant() const {
            return ENUMBITMASK_ISTRUE(this->flags_, PropertyType::CONST);
        };

        [[nodiscard]] bool IsPublic() const {
            return ENUMBITMASK_ISTRUE(this->flags_, PropertyType::PUBLIC);
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
        bool store_wk;

        [[nodiscard]] ArObject *GetObject() {
            if (this->store_wk)
                return ReturnNil(this->weak.GetObject());

            return IncRef(this->value);
        }

        void Cleanup(bool release_key) {
            if (release_key)
                Release(this->key);

            if (this->store_wk) {
                this->weak.DecWeak();
                return;
            }

            Release(this->value);
        }

        void CloneValue(NsEntry *other){
            if(other->store_wk)
                this->weak = other->weak.IncWeak();
            else
                this->value = IncRef(other->value);

            this->store_wk = other->store_wk;
            this->info = other->info;
        }
    };

    struct Namespace : ArObject {
        HMap hmap;
    };

    extern const TypeInfo *type_namespace_;

    Namespace *NamespaceNew();

    Namespace *NamespaceNew(Namespace *ns, PropertyType ignore);

    ArObject *NamespaceGetValue(Namespace *ns, ArObject *key, PropertyInfo *info);

    bool NamespaceMerge(Namespace *dst, Namespace *src);

    bool NamespaceMergePublic(Namespace *dst, Namespace *src);

    bool NamespaceNewSymbol(Namespace *ns, ArObject *key, ArObject *value, PropertyType info);

    bool NamespaceNewSymbol(Namespace *ns, const char *key, ArObject *value, PropertyType info);

    bool NamespaceSetValue(Namespace *ns, ArObject *key, ArObject *value);

    bool NamespaceContains(Namespace *ns, ArObject *key, PropertyInfo *info);

    int NamespaceSetPositional(Namespace *ns, ArObject **values, ArSize count);

} // namespace argon::object

#endif // !ARGON_OBJECT_NAMESPACE_H_
