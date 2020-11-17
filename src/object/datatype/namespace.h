// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_NAMESPACE_H_
#define ARGON_OBJECT_NAMESPACE_H_

#include <object/arobject.h>
#include <utils/enum_bitmask.h>

#define ARGON_OBJECT_NS_INITIAL_SIZE   16
#define ARGON_OBJECT_NS_LOAD_FACTOR    0.75f
#define ARGON_OBJECT_NS_MUL_FACTOR     (ARGON_OBJECT_NS_LOAD_FACTOR * 2)

namespace argon::object {
    enum class PropertyType : unsigned char {
        CONST = 0x01,
        PUBLIC = ((unsigned char) 0x01 << (unsigned char) 1),
        MEMBER = ((unsigned char) 0x01 << (unsigned char) 2),
        WEAK = ((unsigned char) 0x01 << (unsigned char) 3)
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

    struct NsEntry {
        NsEntry *next = nullptr;
        NsEntry *iter_next = nullptr;
        NsEntry *iter_prev = nullptr;

        ArObject *key = nullptr;
        union {
            RefCount ref;
            ArObject *obj;
        };
        PropertyInfo info;
    };

    struct Namespace : ArObject {
        NsEntry **ns;
        NsEntry *iter_begin;
        NsEntry *iter_end;

        size_t cap;
        size_t len;
    };

    extern const TypeInfo type_namespace_;

    Namespace *NamespaceNew();

    bool NamespaceNewSymbol(Namespace *ns, PropertyInfo info, ArObject *key, ArObject *value);

    bool NamespaceSetValue(Namespace *ns, ArObject *key, ArObject *value);

    bool NamespaceContains(Namespace *ns, ArObject *key, PropertyInfo *info);

    ArObject *NamespaceGetValue(Namespace *ns, ArObject *key, PropertyInfo *info);

    void NamespaceRemove(Namespace *ns, ArObject *key);

} // namespace argon::object

#endif // !ARGON_OBJECT_NAMESPACE_H_
